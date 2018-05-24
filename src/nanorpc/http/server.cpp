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
#include "nanorpc/http/server.h"

// THIS
#include "detail/constants.h"
#include "detail/utility.h"

namespace nanorpc::http
{
namespace detail
{

class session final
    : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::ip::tcp::socket socket, core::type::executor_map const &executors,
            core::type::error_handler const &error_handler)
        : executors_{executors}
        , error_handler_{error_handler}
        , socket_{std::move(socket)}
        , strand_{socket_.get_executor()}
    {
    }

    void run() noexcept
    {
        utility::post(socket_.get_executor().context(), [self = shared_from_this()] { self->read(); }, error_handler_ );
    }

private:
    using request_type = boost::beast::http::request<boost::beast::http::string_body>;
    using response_type = boost::beast::http::response<boost::beast::http::string_body>;

    core::type::executor_map const &executors_;
    core::type::error_handler const &error_handler_;

    boost::beast::flat_buffer buffer_;
    request_type request_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    void read()
    {
        request_ = {};

        boost::beast::http::async_read(socket_, buffer_, request_,
                boost::asio::bind_executor(strand_,
                        std::bind(&session::on_read, shared_from_this(), std::placeholders::_1)
                    )
            );
    }


    void on_read(boost::system::error_code const &ec) noexcept
    {
        try
        {
            if (ec == boost::asio::error::operation_aborted)
                return;

            if (ec == boost::beast::http::error::end_of_stream)
            {
                close();
                return;
            }

            auto const keep_alive = request_.keep_alive();

            handle_request(std::move(request_));

            if (keep_alive && socket_.is_open())
                read();

            if (!keep_alive && socket_.is_open())
                close();
        }
        catch (std::exception const &e)
        {
            utility::handle_error<exception::server>(error_handler_, e,
                    "[nanorpc::http::detail::server::session::on_read] ",
                    "Failed to handle request.");
            close();
        }
    }

    void close()
    {
        if (!socket_.is_open())
            return;

        utility::post(socket_.get_io_context(),
                [self = shared_from_this()]
                {
                    boost::system::error_code ec;
                    self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                    if (ec)
                    {
                        utility::handle_error<exception::server>(self->error_handler_,
                                std::make_exception_ptr(std::runtime_error{ec.message()}),
                                "[nanorpc::http::detail::server::session::close] ",
                                "Failed to close socket.");
                    }
                },
                error_handler_
            );
    }

    void handle_request(request_type req)
    {
        auto const target = req.target().to_string();

        auto reply = [self = shared_from_this()] (auto resp)
            {
                auto response = std::make_shared<response_type>(std::move(resp));
                boost::beast::http::async_write(self->socket_, *response,
                        boost::asio::bind_executor(self->strand_,
                                std::bind(&session::on_write, self, std::placeholders::_1, response)
                            )
                    );
            };

        auto const ok =
            [&req](core::type::buffer buffer)
            {
                response_type res{boost::beast::http::status::ok, req.version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req.keep_alive());
                res.body().assign({begin(buffer), end(buffer)});
                res.prepare_payload();
                return res;
            };

        auto const not_found = [&req, &target]
            {
                response_type res{boost::beast::http::status::not_found, req.version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req.keep_alive());
                res.body() = "The resource \"" + target + "\" was not found.";
                res.prepare_payload();
                return res;
            };

        auto const server_error =
            [&req](boost::beast::string_view what)
            {
                response_type res{boost::beast::http::status::internal_server_error, req.version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req.keep_alive());
                res.body() = "An error occurred: \"" + what.to_string() + "\"";
                res.prepare_payload();
                return res;
            };

        auto const bad_request =
        [&req](boost::beast::string_view why)
            {
                response_type res{boost::beast::http::status::bad_request, req.version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req.keep_alive());
                res.body() = why.to_string();
                res.prepare_payload();
                return res;
            };

        auto const iter = executors_.find(target);
        if (iter == end(executors_))
        {
            utility::handle_error<exception::server>(error_handler_,
                    "[nanorpc::http::detail::server::session::handle_request] ",
                    "Resource \"", target, "\" not found.");

            reply(not_found());

            return;
        }

        auto &executor = iter->second;
        if (!executor)
        {
            utility::handle_error<exception::server>(error_handler_,
                    "[nanorpc::http::detail::server::session::handle_request] ",
                    "No exicutor.");

            reply(server_error("Empty exicutor."));

            return;
        }


        auto const &content = req.body();
        if (content.empty())
        {
            utility::handle_error<exception::server>(error_handler_,
                    "[nanorpc::http::detail::server::session::handle_request] ",
                    "The request has no a content.");

            reply(bad_request("No content."));

            return;
        }


        try
        {
            auto response_data = executor({begin(content), end(content)});
            reply(ok(std::move(response_data)));
        }
        catch (std::exception const &e)
        {
            reply(server_error("Handling error."));

            utility::handle_error<exception::server>(error_handler_, e,
                    "[nanorpc::http::detail::server::session::handle_request] ",
                    "Failed to handler request.");

            return;
        }
    }

    void on_write(boost::system::error_code const &ec, std::shared_ptr<response_type>)
    {
        if (!ec)
            return;

        utility::handle_error<exception::server>(error_handler_,
                std::make_exception_ptr(std::runtime_error{ec.message()}),
                "[nanorpc::http::detail::server::session::close] ",
                "Failed to write data.");

        close();
    }
};


class listener final
    : public std::enable_shared_from_this<listener>
{
public:
    listener(boost::asio::io_context &context, boost::asio::ip::tcp::endpoint const &endpoint,
            core::type::executor_map &executors, core::type::error_handler &error_handler)
        : executors_{executors}
        , error_handler_{error_handler}
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
        utility::post(context_, [self = shared_from_this()] { self->accept(); }, error_handler_ );
    }

private:
    core::type::executor_map &executors_;
    core::type::error_handler &error_handler_;

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
                            {
                                utility::handle_error<exception::server>(self->error_handler_,
                                        std::make_exception_ptr(std::runtime_error{ec.message()}),
                                        "[nanorpc::http::detail::listener::accept] ",
                                        "Failed to accept connection.");
                            }
                            else
                            {
                                std::make_shared<session>(std::move(self->socket_), self->executors_, self->error_handler_)->run();
                            }
                        }
                        catch (std::exception const &e)
                        {
                            utility::handle_error<exception::server>(self->error_handler_, e,
                                    "[nanorpc::http::detail::listener::accept] ",
                                    "Failed to process the accept method.");
                        }
                        utility::post(self->context_, [self] { self->accept(); }, self->error_handler_ );
                    }
                );
        }
        catch (std::exception const &e)
        {
            utility::handle_error<exception::server>(error_handler_, e,
                    "[nanorpc::http::detail::listener::accept] ",
                    "Failed to call asynk_accept.");
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

    impl(std::string_view address, std::string_view port, std::size_t workers,
            core::type::executor_map executors, core::type::error_handler error_handler)
        : executors_{std::move(executors)}
        , error_handler_{std::move(error_handler)}
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
            detail::utility::handle_error<exception::server>(error_handler_, e,
                    "[nanorpc::http::server::~impl] ",
                    "Failed to stop server.");
        }
    }

    void run()
    {
        if (!stopped())
            throw std::runtime_error{"[" + std::string{__func__ } + "] Already running."};

        auto listener = std::make_shared<detail::listener>(context_, endpoint_, executors_, error_handler_);
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
                            detail::utility::handle_error<exception::server>(self->error_handler_, e,
                                    "[nanorpc::http::server::run] ",
                                    "Failed to run server.");

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
                        detail::utility::handle_error<exception::server>(error_handler_, e,
                                "[nanorpc::http::server::stop] ",
                                "Failed to stop server.");

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

    core::type::executor_map executors_;
    core::type::error_handler error_handler_;

    int workers_count_;
    boost::asio::io_context context_;
    boost::asio::ip::tcp::endpoint endpoint_;
    std::shared_ptr<detail::listener> listener_;
    threads_type workers_;
};

server::server(std::string_view address, std::string_view port, std::size_t workers,
        core::type::executor_map executors, core::type::error_handler error_handler)
    : impl_{std::make_shared<impl>(std::move(address), std::move(port), workers,
            std::move(executors), std::move(error_handler))}
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
