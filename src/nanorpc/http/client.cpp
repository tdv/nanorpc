//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

// STD
#include <cstdint>
#include <cstdlib>
#include <stdexcept>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <utility>
#include <vector>

// BOOST
#include <boost/asio.hpp>
#include <boost/beast.hpp>

// NANORPC
#include "nanorpc/http/client.h"

// THIS
#include "detail/constants.h"
#include "detail/utility.h"

namespace nanorpc::http
{
namespace detail
{
namespace
{

class session final
    : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::ip::tcp::socket socket, core::type::error_handler const &error_handler)
        : socket_{std::move(socket)}
        , error_handler_{error_handler}
    {
    }

    auto connect(boost::asio::ip::tcp::resolver::results_type const &endpoints_)
    {
        std::promise<std::shared_ptr<session>> promise;

        auto connect = [self = shared_from_this(), &promise, &endpoints_]
            {
                boost::asio::async_connect(self->socket_, std::begin(endpoints_), std::end(endpoints_),
                        [self, &promise] (boost::system::error_code const &ec, auto)
                        {
                            if (ec)
                            {
                                auto exception = exception::client{"Failed to connect to remote host. " + ec.message()};
                                promise.set_exception(std::make_exception_ptr(std::move(exception)));
                                return;
                            }

                            promise.set_value(std::move(self));
                        }
                    );
            };

        utility::post(socket_.get_io_context(), std::move(connect));

        return promise.get_future().get();
    }

    void close() noexcept
    {
        auto close_connection = [self = shared_from_this()]
            {
                if (!self->socket_.is_open())
                    return;
                boost::system::error_code ec;
                self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                if (ec)
                {
                    utility::handle_error<exception::client>(self->error_handler_,
                            std::make_exception_ptr(exception::client{ec.message()}),
                            "[nanorpc::http::detail::client::session::close] ",
                            "Failed to shutdown session.");
                }
                self->socket_.close(ec);
                if (ec)
                {
                    utility::handle_error<exception::client>(self->error_handler_,
                            std::make_exception_ptr(exception::client{ec.message()}),
                            "[nanorpc::http::detail::client::session::close] ",
                            "Failed to close session.");
                }
            };

        utility::post(socket_.get_io_context(), std::move(close_connection));
    }

    core::type::buffer send(core::type::buffer const &buffer, std::string const &location, std::string const &host)
    {
        auto request = std::make_shared<boost::beast::http::request<boost::beast::http::string_body>>();

        request->keep_alive(true);
        request->body().assign(begin(buffer), end(buffer));

        request->version(constants::http_version);
        request->method(boost::beast::http::verb::post);
        request->target(location);
        request->set(boost::beast::http::field::host, host);
        request->set(boost::beast::http::field::user_agent, constants::user_agent_name);
        request->set(boost::beast::http::field::content_length, buffer.size());
        request->set(boost::beast::http::field::content_type, constants::content_type);
        request->set(boost::beast::http::field::keep_alive, request->keep_alive());

        auto promise = std::make_shared<std::promise<core::type::buffer>>();

        auto receive_response = [self = shared_from_this(), promise]
            {
                auto buffer = std::make_shared<boost::beast::flat_buffer>();
                auto response = std::make_shared<boost::beast::http::response<boost::beast::http::string_body>>();

                boost::beast::http::async_read(self->socket_, *buffer, *response,
                        [self, promise, buffer, response]
                        (boost::system::error_code const &ec, std::size_t bytes)
                        {
                            boost::ignore_unused(bytes);
                            if (ec)
                            {
                                auto exception = exception::client{"Failed to receive response. " + ec.message()};
                                promise->set_exception(std::make_exception_ptr(std::move(exception)));
                                self->close();
                                return;
                            }

                            auto const &content = response->body();
                            promise->set_value({begin(content), end(content)});
                        }
                    );
            };


        auto post_request = [self = shared_from_this(), promise, request, recv = std::move(receive_response)]
            {
                boost::beast::http::async_write(self->socket_, *request,
                        [self, promise, receive = std::move(recv)]
                        (boost::system::error_code const &ec, std::size_t bytes)
                        {
                            boost::ignore_unused(bytes);
                            if (ec)
                            {
                                auto exception = exception::client{"Failed to post request. " + ec.message()};
                                promise->set_exception(std::make_exception_ptr(std::move(exception)));
                                self->close();
                                return;
                            }

                            utility::post(self->socket_.get_io_context(), std::move(receive));
                        }
                    );

            };

        utility::post(socket_.get_io_context(), std::move(post_request));

        return std::move(promise->get_future().get());
    }

private:
    boost::asio::ip::tcp::socket socket_;
    core::type::error_handler const &error_handler_;
};

}   // namespace
}   // namespace detail

class client::impl final
    : public std::enable_shared_from_this<impl>
{
public:
    impl(std::string_view host, std::string_view port, std::size_t workers, core::type::error_handler error_handler)
        : error_handler_{std::move(error_handler)}
        , workers_count_{std::max<int>(1, workers)}
        , context_{workers_count_}
        , work_guard_{boost::asio::make_work_guard(context_)}
    {
        {
            boost::system::error_code ec;
            boost::asio::ip::tcp::resolver resolver{context_};
            endpoints_ = resolver.resolve(host, port, ec);
            if (ec)
                throw exception::client{"Failed to resolve endpoint \"" + std::string{host} + ":" + std::string{port} + "\""};
        }
    }

    ~impl() noexcept
    {
        try
        {
            if (stopped())
                return;

            stop();
        }
        catch (std::exception const &e)
        {
            detail::utility::handle_error<exception::client>(error_handler_, e,
                    "[nanorpc::client::impl::~impl] Failed to done.");
        }
    }

    void init_executor(std::string_view location)
    {
        auto executor = [this_ = std::weak_ptr{shared_from_this()}, dest_location = std::string{location}, host = boost::asio::ip::host_name()]
            (core::type::buffer request)
            {
                auto self = this_.lock();
                if (!self)
                    throw exception::client{"No owner object."};

                session_ptr session;
                core::type::buffer response;
                try
                {
                    session = self->get_session();
                    try
                    {
                        response = session->send(request, dest_location, host);
                    }
                    catch (exception::client const &e)
                    {
                        detail::utility::handle_error<exception::client>(self->error_handler_, std::exception{e},
                                "[nanorpc::client::impl::executor] Failed to execute request. Try again ...");

                        session = self->get_session();
                        response = session->send(std::move(request), dest_location, host);
                    }
                    self->put_session(std::move(session));
                }
                catch (...)
                {
                    if (session)
                        session->close();

                    auto exception = exception::client{"[nanorpc::client::impl::executor] Failed to send data."};
                    std::throw_with_nested(std::move(exception));
                }
                return response;
            };

        executor_ = std::move(executor);
    }

    void run()
    {
        if (!stopped())
            throw exception::client{"Already running."};

        threads_type workers;
        workers.reserve(workers_count_);

        for (auto i = workers_count_ ; i ; --i)
        {
            workers.emplace_back(
                    [self = this]
                    {
                        try
                        {
                            self->context_.run();
                        }
                        catch (std::exception const &e)
                        {
                            detail::utility::handle_error<exception::client>(self->error_handler_, e,
                                    "[nanorpc::client::impl::run] Failed to run.");
                            std::exit(EXIT_FAILURE);
                        }
                    }
                );
        }

        workers_ = std::move(workers);
    }

    void stop()
    {
        if (stopped())
            throw exception::client{"Not runned."};

        work_guard_.reset();
        context_.stop();
        std::exchange(session_queue_, session_queue_type{});
        for_each(begin(workers_), end(workers_), [&] (std::thread &t)
                {
                    try
                    {
                        t.join();
                    }
                    catch (std::exception const &e)
                    {
                        detail::utility::handle_error<exception::client>(error_handler_, e,
                                "[nanorpc::client::impl::stop] Failed to stop.");
                        std::exit(EXIT_FAILURE);
                    }
                }
            );

        workers_.clear();
    }

    bool stopped() const noexcept
    {
        return workers_.empty();
    }

    core::type::executor const& get_executor() const
    {
        return executor_;
    }

private:
    using session_ptr = std::shared_ptr<detail::session>;
    using session_queue_type = std::queue<session_ptr>;

    using threads_type = std::vector<std::thread>;

    core::type::executor executor_;

    core::type::error_handler error_handler_;
    int workers_count_;
    boost::asio::io_context context_;
    boost::asio::executor_work_guard<boost::asio::io_context::executor_type> work_guard_;
    boost::asio::ip::tcp::resolver::results_type endpoints_;

    std::mutex lock_;
    session_queue_type session_queue_;

    threads_type workers_;

    session_ptr get_session()
    {
        session_ptr session;

        {
            std::lock_guard lock{lock_};
            if (!session_queue_.empty())
            {
                session = session_queue_.front();
                session_queue_.pop();
            }
        }

        if (!session)
        {
            if (stopped())
                throw exception::client{"Failed to get session. The client was not started."};
            boost::asio::ip::tcp::socket socket{context_};
            std::exchange(session, std::make_shared<detail::session>(std::move(socket), error_handler_)->connect(endpoints_));
        }

        return session;
    }

    void put_session(session_ptr session)
    {
        std::lock_guard lock{lock_};
        session_queue_.emplace(std::move(session));
    }
};

client::client(std::string_view host, std::string_view port, std::size_t workers, std::string_view location,
        core::type::error_handler error_handler)
    : impl_{std::make_shared<impl>(std::move(host), std::move(port), workers, std::move(error_handler))}
{
    impl_->init_executor(std::move(location));
}

client::~client() noexcept
{
    impl_.reset();
}

void client::run()
{
    impl_->run();
}

void client::stop()
{
    impl_->stop();
}

bool client::stopped() const noexcept
{
    return impl_->stopped();
}

core::type::executor const& client::get_executor() const
{
    return impl_->get_executor();
}

}   // namespace nanorpc::http
