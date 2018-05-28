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
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>

// NANORPC
#include "nanorpc/http/server.h"
#include "nanorpc/https/server.h"

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
    using ssl_context_ptr = std::shared_ptr<boost::asio::ssl::context>;

    session(ssl_context_ptr ssl_context, boost::asio::ip::tcp::socket socket, core::type::executor_map const &executors,
                core::type::error_handler const &error_handler)
        : executors_{executors}
        , error_handler_{error_handler}
        , socket_{std::move(socket)}
        , ssl_context_{std::move(ssl_context)}
        , strand_{socket_.get_executor()}
    {
        if (ssl_context_)
            ssl_stream_ = std::make_unique<ssl_stream_type>(socket_, *ssl_context_);
    }

    void run() noexcept
    {
        if (!ssl_context_)
        {
            utility::post(socket_.get_executor().context(), [self = shared_from_this()] { self->read(); }, error_handler_ );
        }
        else
        {
            ssl_stream_->async_handshake(boost::asio::ssl::stream_base::server, boost::asio::bind_executor(strand_,
                    [self = shared_from_this()] (boost::system::error_code const &ec)
                    {
                        if (ec)
                        {
                            utility::handle_error<exception::server>(self->error_handler_,
                                    std::make_exception_ptr(std::runtime_error{ec.message()}),
                                    "[nanorpc::http::detail::server::session::run] ",
                                    "Failed to do handshake.");

                            self->close();

                            return;
                        }

                        utility::post(self->socket_.get_executor().context(), [self] { self->read(); }, self->error_handler_ );
                    } ));
        }
    }

private:
    using ssl_stream_type = boost::asio::ssl::stream<boost::asio::ip::tcp::socket &>;

    using request_type = boost::beast::http::request<boost::beast::http::string_body>;
    using response_type = boost::beast::http::response<boost::beast::http::string_body>;

    core::type::executor_map const &executors_;
    core::type::error_handler const &error_handler_;

    boost::beast::flat_buffer buffer_;
    request_type request_;
    boost::asio::ip::tcp::socket socket_;
    ssl_context_ptr ssl_context_;
    std::unique_ptr<ssl_stream_type> ssl_stream_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    void read()
    {
        request_ = {};

        auto do_read = [this] (auto &source)
            {
                boost::beast::http::async_read(source, buffer_, request_,
                        boost::asio::bind_executor(strand_,
                                std::bind(&session::on_read, shared_from_this(), std::placeholders::_1)
                            )
                    );
            };

        if (!ssl_stream_)
            do_read(socket_);
        else
            do_read(*ssl_stream_);
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
        if (!socket_.is_open() || (ssl_stream_ && !ssl_stream_->next_layer().is_open()))
            return;

        utility::post(socket_.get_io_context(),
                [self = shared_from_this()]
                {
                    boost::system::error_code ec;
                    if (!self->ssl_stream_)
                        self->socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
                    else
                        self->ssl_stream_->shutdown(ec);
                    if (ec)
                    {
                        if (ec != boost::asio::error::operation_aborted)
                        {
                            utility::handle_error<exception::server>(self->error_handler_,
                                    std::make_exception_ptr(std::runtime_error{ec.message()}),
                                    "[nanorpc::http::detail::server::session::close] ",
                                    "Failed to close socket.");
                        }
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

                auto do_write = [&] (auto &source)
                    {
                        boost::beast::http::async_write(source, *response,
                                boost::asio::bind_executor(self->strand_,
                                        std::bind(&session::on_write, self, std::placeholders::_1, response)
                                    )
                            );
                    };

                if (!self->ssl_stream_)
                    do_write(self->socket_);
                else
                    do_write(*self->ssl_stream_);
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
                "[nanorpc::http::detail::server::session::on_write] ",
                "Failed to write data.");

        close();
    }
};

class listener final
    : public std::enable_shared_from_this<listener>
{
public:
    listener(session::ssl_context_ptr ssl_context, boost::asio::io_context &context,
            boost::asio::ip::tcp::endpoint const &endpoint,
            core::type::executor_map &executors, core::type::error_handler &error_handler)
        : executors_{executors}
        , error_handler_{error_handler}
        , ssl_context_{std::move(ssl_context)}
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

    session::ssl_context_ptr ssl_context_;
    boost::asio::io_context &context_;
    boost::asio::ip::tcp::acceptor acceptor_;
    boost::asio::ip::tcp::socket socket_;

    void accept() noexcept
    {
        try
        {
            acceptor_.async_accept(socket_,
                    [self = this] (boost::system::error_code const &ec)
                    {
                        try
                        {
                            if (ec)
                            {
                                if (ec != boost::asio::error::operation_aborted)
                                {
                                    utility::handle_error<exception::server>(self->error_handler_,
                                            std::make_exception_ptr(std::runtime_error{ec.message()}),
                                            "[nanorpc::http::detail::listener::accept] ",
                                            "Failed to accept connection.");
                                }
                            }
                            else
                            {
                                std::make_shared<session>(self->ssl_context_, std::move(self->socket_),
                                        self->executors_, self->error_handler_)->run();
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

}   // namespace

class server_impl final
    : public std::enable_shared_from_this<server_impl>
{
public:
    server_impl(server_impl const &) = delete;
    server_impl& operator = (server_impl const &) = delete;

    server_impl(std::string_view address, std::string_view port, std::size_t workers,
            core::type::executor_map executors, core::type::error_handler error_handler)
        : executors_{std::move(executors)}
        , error_handler_{std::move(error_handler)}
        , workers_count_{std::max<int>(1, workers)}
        , context_{workers_count_}
        , endpoint_{boost::asio::ip::make_address(address),
                static_cast<unsigned short>(std::stol(port.data()))}
    {
    }

    server_impl(boost::asio::ssl::context ssl_context, std::string_view address, std::string_view port,
            std::size_t workers, core::type::executor_map executors, core::type::error_handler error_handler)
        : server_impl{std::move(address), std::move(port), workers, std::move(executors), std::move(error_handler)}
    {
        ssl_context_ = std::make_shared<boost::asio::ssl::context>(std::move(ssl_context));
    }



    ~server_impl() noexcept
    {
        if (stopped())
            return;

        try
        {
            stop();
        }
        catch (std::exception const &e)
        {
            utility::handle_error<exception::server>(error_handler_, e,
                    "[nanorpc::http::server::~sserver_impl] ",
                    "Failed to stop server.");
        }
    }

    void run()
    {
        if (!stopped())
            throw std::runtime_error{"[" + std::string{__func__ } + "] Already running."};

        auto new_listener = std::make_shared<listener>(ssl_context_, context_, endpoint_, executors_, error_handler_);
        new_listener->run();

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
                            utility::handle_error<exception::server>(self->error_handler_, e,
                                    "[nanorpc::http::server::run] ",
                                    "Failed to run server.");

                            std::exit(EXIT_FAILURE);
                        }
                    }
                );
        }

        listener_ = std::move(new_listener);
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
                        utility::handle_error<exception::server>(error_handler_, e,
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

    session::ssl_context_ptr ssl_context_;
    int workers_count_;
    boost::asio::io_context context_;
    boost::asio::ip::tcp::endpoint endpoint_;
    std::shared_ptr<listener> listener_;
    threads_type workers_;
};

}   // namespace detail

server::server(std::string_view address, std::string_view port, std::size_t workers,
        core::type::executor_map executors, core::type::error_handler error_handler)
    : impl_{std::make_shared<detail::server_impl>(std::move(address), std::move(port), workers,
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

namespace nanorpc::https
{

server::server(boost::asio::ssl::context context, std::string_view address, std::string_view port,
        std::size_t workers, core::type::executor_map executors, core::type::error_handler error_handler)
    : impl_{std::make_shared<http::detail::server_impl>(std::move(context), std::move(address), std::move(port),
            workers, std::move(executors), std::move(error_handler))}
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

}   // namespace nanorpc::https
