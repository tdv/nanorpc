//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

// NANORPC

// STD
#include <cstdlib>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>

// BOOST
#include <boost/asio.hpp>
#include <boost/beast.hpp>

// NANORPC
#include "nanorpc/core/detail/config.h"
#include "nanorpc/http/server.h"

#ifdef NANORPC_WITH_SSL

// BOOST
#include <boost/asio/ssl.hpp>

// NANORPC
#include "nanorpc/https/server.h"

#endif  // !NANORPC_WITH_SSL

// THIS
#include "detail/constants.h"
#include "detail/utility.h"

namespace nanorpc::http
{
namespace detail
{
namespace
{

class session
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

    virtual ~session() noexcept = default;

    void run() noexcept
    {
        auto self = shared_from_this();

        auto on_handshake = [self] (boost::system::error_code const &ec)
            {
                if (!ec)
                {
                    utility::post(self->socket_.get_io_context(), [self] { self->read(); }, self->error_handler_ );
                }
                else
                {
                    utility::handle_error<exception::server>(self->error_handler_,
                    std::make_exception_ptr(std::runtime_error{ec.message()}),
                    "[nanorpc::http::detail::server::session::run] ",
                    "Failed to do handshake.");

                    self->close();
                }
            };

        utility::post(socket_.get_io_context(), [self, func = std::move(on_handshake)]
                { self->handshake(std::move(func)); }, error_handler_ );

    }

protected:
    using socket_type = boost::asio::ip::tcp::socket;
    using strand_type = boost::asio::strand<boost::asio::io_context::executor_type>;

    using buffer_type = boost::beast::flat_buffer;
    using buffer_ptr = std::shared_ptr<buffer_type>;

    using request_type = boost::beast::http::request<boost::beast::http::string_body>;
    using request_ptr = std::shared_ptr<request_type>;

    using response_type = boost::beast::http::response<boost::beast::http::string_body>;
    using response_ptr = std::shared_ptr<response_type>;

    using on_completed_func = std::function<void (boost::system::error_code const &)>;

    socket_type& get_socket()
    {
        return socket_;
    }

    strand_type& get_strand()
    {
        return strand_;
    }

    virtual void handshake(on_completed_func on_handshake) = 0;
    virtual void close(boost::system::error_code &ec) = 0;
    virtual void read(buffer_ptr, request_ptr, on_completed_func on_read) = 0;
    virtual void write(response_ptr response, on_completed_func on_write) = 0;

private:
    core::type::executor_map const &executors_;
    core::type::error_handler const &error_handler_;

    socket_type socket_;
    strand_type strand_;

    void read()
    {
        auto buffer = std::make_shared<buffer_type>();
        auto request = std::make_shared<request_type>();

        auto on_read = [self = shared_from_this(), request] (boost::system::error_code const &ec) noexcept
            {
                try
                {
                    if (ec == boost::asio::error::operation_aborted)
                        return;

                    if (ec == boost::beast::http::error::end_of_stream)
                    {
                        self->close();
                        return;
                    }

                    auto const keep_alive = request->keep_alive();
                    auto const need_eof = request->need_eof();

                    self->handle_request(std::move(request));

                    if (keep_alive && self->get_socket().is_open())
                        self->read();

                    if ((need_eof || !keep_alive) && self->get_socket().is_open())
                        self->close();
                }
                catch (std::exception const &e)
                {
                    utility::handle_error<exception::server>(self->error_handler_, e,
                            "[nanorpc::http::detail::server::session::read] ",
                            "Failed to handle request.");
                    self->close();
                }
            };

        read(buffer, request, std::move(on_read));
    }

    void close()
    {
        if (!get_socket().is_open())
            return;

        utility::post(socket_.get_io_context(),
                [self = shared_from_this()]
                {
                    boost::system::error_code ec;
                    self->close(ec);
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

    void handle_request(request_ptr req)
    {
        auto const target = req->target().to_string();
        auto const need_eof = req->need_eof();

        auto reply = [self = shared_from_this()] (auto resp)
            {
                auto response = std::make_shared<response_type>(std::move(resp));

                auto on_write = [self] (boost::system::error_code const &ec)
                    {
                        if (ec == boost::asio::error::operation_aborted || ec == boost::asio::error::broken_pipe)
                            return;

                        if (!ec)
                            return;

                        utility::handle_error<exception::server>(self->error_handler_,
                                std::make_exception_ptr(std::runtime_error{ec.message()}),
                                "[nanorpc::http::detail::server::session::on_write] ",
                                "Failed to write data.");

                        self->close();
                    };

                self->write(response, std::move(on_write) );
            };

        auto const ok =
            [&req](core::type::buffer buffer)
            {
                response_type res{boost::beast::http::status::ok, req->version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req->keep_alive() && !req->need_eof());
                res.body().assign({begin(buffer), end(buffer)});
                res.prepare_payload();
                return res;
            };

        auto const not_found = [&req, &target]
            {
                response_type res{boost::beast::http::status::not_found, req->version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req->keep_alive() && !req->need_eof());
                res.body() = "The resource \"" + target + "\" was not found.";
                res.prepare_payload();
                return res;
            };

        auto const server_error =
            [&req](boost::beast::string_view what)
            {
                response_type res{boost::beast::http::status::internal_server_error, req->version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req->keep_alive() && !req->need_eof());
                res.body() = "An error occurred: \"" + what.to_string() + "\"";
                res.prepare_payload();
                return res;
            };

        auto const bad_request =
        [&req](boost::beast::string_view why)
            {
                response_type res{boost::beast::http::status::bad_request, req->version()};
                res.set(boost::beast::http::field::server, constants::server_name);
                res.set(boost::beast::http::field::content_type, constants::content_type);
                res.keep_alive(req->keep_alive() && !req->need_eof());
                res.body() = why.to_string();
                res.prepare_payload();
                return res;
            };

        if (target.empty())
        {
            reply(not_found());
            return;
        }

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


        auto const &content = req->body();
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

            if (need_eof)
                close();
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
};

class listener final
    : public std::enable_shared_from_this<listener>
{
public:
    using session_ptr = std::shared_ptr<session>;
    using session_factory  = std::function<session_ptr (boost::asio::ip::tcp::socket,
            core::type::executor_map const &, core::type::error_handler const &)>;

    listener(boost::asio::io_context &context, boost::asio::ip::tcp::endpoint const &endpoint,
            session_factory make_session,
            core::type::executor_map &executors, core::type::error_handler &error_handler)
        : make_session_{std::move(make_session)}
        , executors_{executors}
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
    session_factory make_session_;
    core::type::executor_map const &executors_;
    core::type::error_handler const &error_handler_;

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
                                self->make_session_(std::move(self->socket_),
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

class server
    : public std::enable_shared_from_this<server>
{
public:
    server(server const &) = delete;
    server& operator = (server const &) = delete;

    server(std::string_view address, std::string_view port, std::size_t workers,
            core::type::executor_map executors, core::type::error_handler error_handler)
        : executors_{std::move(executors)}
        , error_handler_{std::move(error_handler)}
        , workers_count_{std::max<int>(1, workers)}
        , context_{workers_count_}
        , endpoint_{boost::asio::ip::make_address(address),
                static_cast<unsigned short>(std::stol(port.data()))}
    {
    }

    virtual ~server() noexcept
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
                    "[nanorpc::http::server::~sserver] ",
                    "Failed to stop server.");
        }
    }

    void run()
    {
        if (!stopped())
            throw std::runtime_error{"[" + std::string{__func__ } + "] Already running."};

        auto factory = std::bind(&server::make_session, this, std::placeholders::_1, std::placeholders::_2, std::placeholders::_3);
        auto new_listener = std::make_shared<listener>(context_, endpoint_, std::move(factory), executors_, error_handler_);
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

protected:
    using session_ptr = listener::session_ptr;

    virtual session_ptr make_session(boost::asio::ip::tcp::socket socket,
            core::type::executor_map const &executors,
            core::type::error_handler const &error_handler) = 0;

private:
    using threads_type = std::vector<std::thread>;

    core::type::executor_map executors_;
    core::type::error_handler error_handler_;

    int workers_count_;
    boost::asio::io_context context_;
    boost::asio::ip::tcp::endpoint endpoint_;
    std::shared_ptr<listener> listener_;
    threads_type workers_;
};

}   // namespace
}   // namespace detail

class server::impl final
    : public detail::server
{
public:
    using server::server;

private:
    virtual session_ptr make_session(boost::asio::ip::tcp::socket socket,
            core::type::executor_map const &executors,
            core::type::error_handler const &error_handler) override final
    {
        return std::make_shared<session>(std::move(socket), executors, error_handler);
    }

    class session final
        : public detail::session
    {
    public:
        using detail::session::session;

    private:
        virtual void handshake(on_completed_func on_handshake) override final
        {
            on_handshake(boost::system::error_code{});
        }

        virtual void close(boost::system::error_code &ec) override final
        {
            get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
        }

        virtual void read(buffer_ptr buffer, request_ptr request, on_completed_func on_read) override final
        {
            boost::beast::http::async_read(get_socket(), *buffer, *request,
                    boost::asio::bind_executor(get_strand(),
                            [func = std::move(on_read), buffer, request]
                            (boost::system::error_code const &ec, auto)
                            { func(ec); }
                        )
                    );
        }

        virtual void write(response_ptr response, on_completed_func on_write) override final
        {
            boost::beast::http::async_write(get_socket(), *response,
                    boost::asio::bind_executor(get_strand(),
                            [func = std::move(on_write), response]
                            (boost::system::error_code const &ec, auto) { func(ec); }
                        )
                );
        }
    };
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

#ifdef NANORPC_WITH_SSL

namespace nanorpc::https
{

class server::impl final
    : public http::detail::server
{
public:
    impl(boost::asio::ssl::context ssl_context, std::string_view address, std::string_view port,
            std::size_t workers, core::type::executor_map executors, core::type::error_handler error_handler)
        : server{std::move(address), std::move(port), workers, std::map(executors), std::move(error_handler)}
        , ssl_context_{std::move(ssl_context)}
    {
    }

private:
    boost::asio::ssl::context ssl_context_;

    virtual session_ptr make_session(boost::asio::ip::tcp::socket socket,
            core::type::executor_map const &executors,
            core::type::error_handler const &error_handler) override final
    {
        return std::make_shared<session>(ssl_context_, std::move(socket), executors, error_handler);
    }

    class session final
        : public http::detail::session
    {
    public:
        session(boost::asio::ssl::context &ssl_context, boost::asio::ip::tcp::socket socket,
                core::type::executor_map const &executors, core::type::error_handler const &error_handler)
            : http::detail::session{std::move(socket), executors, error_handler}
            , stream_{std::in_place, get_socket(), ssl_context}
        {
        }

        virtual ~session() noexcept override
        {
            stream_.reset();
        }

    private:
        std::optional<boost::asio::ssl::stream<boost::asio::ip::tcp::socket&>> stream_;

        virtual void handshake(on_completed_func on_handshake) override final
        {
            stream_->async_handshake(boost::asio::ssl::stream_base::server, boost::asio::bind_executor(get_strand(),
                    [func = std::move(on_handshake)] (boost::system::error_code const &ec) { func(ec); } ));
        }

        virtual void close(boost::system::error_code &ec) override final
        {
            boost::ignore_unused(ec);
            stream_->async_shutdown([] (boost::system::error_code const &ec)
                    { boost::ignore_unused(ec); } );
        }

        virtual void read(buffer_ptr buffer, request_ptr request, on_completed_func on_read) override final
        {
            boost::beast::http::async_read(*stream_, *buffer, *request,
                    boost::asio::bind_executor(get_strand(),
                            [func = std::move(on_read), buffer, request]
                            (boost::system::error_code const &ec, auto)
                            { func(ec); }
                        )
                    );
        }

        virtual void write(response_ptr response, on_completed_func on_write) override final
        {
            boost::beast::http::async_write(*stream_, *response,
                    boost::asio::bind_executor(get_strand(),
                            [func = std::move(on_write), response]
                            (boost::system::error_code const &ec, auto) { func(ec); }
                        )
                );
        }
    };
};

server::server(boost::asio::ssl::context context, std::string_view address, std::string_view port,
        std::size_t workers, core::type::executor_map executors, core::type::error_handler error_handler)
    : impl_{std::make_shared<impl>(std::move(context), std::move(address), std::move(port),
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

#endif  // !NANORPC_WITH_SSL
