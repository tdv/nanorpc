//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

// STD
#include <cstdlib>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// BOOST
#include <boost/asio.hpp>
#include <boost/beast.hpp>

// NANORPC
#include <nanorpc/http/server.h>

// THIS
#include "detail/data.h"
#include "detail/session.h"
#include "detail/utility.h"

namespace nanorpc::http
{
namespace detail
{

class listener final
    : public std::enable_shared_from_this<listener>
{
public:
    listener(boost::asio::io_context &context, boost::asio::ip::tcp::endpoint const &endpoint,
            executor_data const &data)
        : data_{data}
        , context_{context}
        , acceptor_{context_}
        , socket_{context_}
    {
        boost::system::error_code ec;

        acceptor_.open(endpoint.protocol(), ec);
        if (ec)
            throw std::runtime_error{"Failed to open acceptor. Message: " + ec.message()};

        acceptor_.set_option(boost::asio::socket_base::reuse_address{true}, ec);
        if (ec)
            throw std::runtime_error{"Failed to set option \"reuse_address\". Message: " + ec.message()};

        acceptor_.bind(endpoint, ec);
        if (ec)
            throw std::runtime_error{"Failed to bind acceptor. Message: " + ec.message()};

        acceptor_.listen(boost::asio::socket_base::max_listen_connections, ec);
        if (ec)
            throw std::runtime_error{"Failed to start listen. Message: " + ec.message()};
    }

    void run()
    {
        utility::post(context_, [self = shared_from_this()] { self->accept(); } );
    }

private:
    executor_data data_;
    boost::asio::io_context &context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;

    void accept() noexcept
    {
        try
        {
            acceptor_.async_accept(socket_,
                    [self = shared_from_this()] (boost::system::error_code const &ec)
                    {
                        try
                        {
                            if (ec)
                                utility::handle_error(self->data_, "Failed to accept connection. Message: " + ec.message());
                            else
                                std::make_shared<session>(std::move(self->socket_), self->data_)->run();
                        }
                        catch (std::exception const &e)
                        {
                            utility::handle_error(self->data_, "Failed to process the accept method. Error: " + std::string{e.what()});
                        }
                        utility::post(self->context_, [self] { self->accept(); } );
                    }
                );
        }
        catch (std::exception const &e)
        {
            utility::handle_error(data_, "Failed to call asynk_accept. Error: " + std::string{e.what()});
        }
    }
};

}   // namespace detail

class server::impl final
    : public std::enable_shared_from_this<impl>
{
public:
    impl(impl const &) = delete;
    impl& operator = (impl const &) = delete;

    impl(std::string_view address, std::string_view port, std::size_t workers, detail::executor_data data)
        : data_{std::move(data)}
        , workers_count_{std::max<int>(1, workers)}
        , context_{workers_count_}
        , endpoint_{boost::asio::ip::make_address(address),
                static_cast<unsigned short>(std::stol(port.data()))}
    {
    }

    ~impl() noexcept
    {
        if (stopped())
            return;

        try
        {
            stop();
        }
        catch (std::exception const &e)
        {
            detail::utility::handle_error(data_, e.what());
        }
    }

    void run()
    {
        if (!stopped())
            throw std::runtime_error{"[" + std::string{__func__ } + "] Already running."};

        auto listener = std::make_shared<detail::listener>(context_, endpoint_, data_);
        listener->run();

        threads_type workers;
        workers.reserve(workers_count_);

        for (auto i = workers_count_ ; i ; --i)
        {
            workers.emplace_back(
                    [self = shared_from_this()]
                    {
                        try
                        {
                            self->context_.run();
                        }
                        catch (std::exception const &e)
                        {
                            detail::utility::handle_error(self->data_, e.what());
                            std::exit(EXIT_FAILURE);
                        }
                    }
                );
        }

        std::exchange(listener, listener_);
        workers_ = std::move(workers);
    }

    void stop()
    {
        if (stopped())
            throw std::runtime_error{"[" + std::string{__func__ } + "] Not runned."};

        listener_.reset();
        context_.stop();
        for_each(begin(workers_), end(workers_), [&] (std::thread &t)
                {
                    try
                    {
                        t.join();
                    }
                    catch (std::exception const &e)
                    {
                        detail::utility::handle_error(data_, e.what());
                        std::exit(EXIT_FAILURE);
                    }
                }
            );

        workers_.clear();
    }

    bool stopped() const noexcept
    {
        return !listener_;
    }

private:
    using threads_type = std::vector<std::thread>;

    detail::executor_data data_;
    int workers_count_;
    boost::asio::io_context context_;
    boost::asio::ip::tcp::endpoint endpoint_;
    std::shared_ptr<detail::listener> listener_;
    threads_type workers_;
};

server::server(std::string_view address, std::string_view port, std::size_t workers,
        core::type::executor_map executors, core::type::error_handler error_handler)
    : impl_{std::make_shared<impl>(std::move(address), std::move(port), workers,
            detail::executor_data{std::move(executors), std::move(error_handler)})}
{
}

server::~server() noexcept
{
}

void server::run()
{
    impl_->run();
}

void server::stop()
{
    impl_->stop();
}

bool server::stopped() const noexcept
{
    return impl_->stopped();
}

}   // namespace nanorpc::http
