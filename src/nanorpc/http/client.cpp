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
#include "nanorpc/core/detail/config.h"
#include "nanorpc/http/client.h"

#ifdef NANORPC_WITH_SSL

// BOOST
#include <boost/asio/ssl.hpp>

// NANORPC
#include "nanorpc/https/client.h"

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
    session(boost::asio::io_context &context, core::type::error_handler const &error_handler)
        : context_{context}
        , error_handler_{error_handler}
    {
    }

    virtual ~session() noexcept = default;

    void connect(boost::asio::ip::tcp::resolver::results_type const &endpoints)
    {
        std::promise<bool> promise;

        auto on_connect = [&promise]
            (boost::system::error_code const &ec)
            {
                if (!ec)
                {
                    promise.set_value(true);
                }
                else
                {
                    auto exception = exception::client{"Failed to connect to remote host. " + ec.message()};
                    promise.set_exception(std::make_exception_ptr(std::move(exception)));
                }
            };

        utility::post(context_,
                [&] { connect(endpoints, std::move(on_connect)); } );

        promise.get_future().get();;
    }

    void close() noexcept
    {
        auto close_connection = [self = shared_from_this()]
            {
                boost::system::error_code ec;
                self->close(ec);
                if (!ec)
                    return;
                utility::handle_error<exception::client>(self->error_handler_,
                        std::make_exception_ptr(exception::client{ec.message()}),
                        "[nanorpc::http::detail::client::session::close] ",
                        "Failed to close session.");
            };

        utility::post(context_, std::move(close_connection));
    }

    core::type::buffer send(core::type::buffer const &buffer, std::string const &location, std::string const &host)
    {
        auto request = std::make_shared<request_type>();

        request->keep_alive(true);
        request->body().assign(begin(buffer), end(buffer));
        request->prepare_payload();

        request->version(constants::http_version);
        request->method(boost::beast::http::verb::post);
        request->target(location);
        request->set(boost::beast::http::field::host, host);
        request->set(boost::beast::http::field::user_agent, constants::user_agent_name);
        request->set(boost::beast::http::field::content_length, buffer.size());
        request->set(boost::beast::http::field::content_type, constants::content_type);
        request->set(boost::beast::http::field::keep_alive, request->keep_alive());

        auto self = shared_from_this();

        auto promise = std::make_shared<std::promise<core::type::buffer>>();

        auto receive_response = [self, promise]
            {
                auto buffer = std::make_shared<buffer_type>();
                auto response = std::make_shared<response_type>();

                self->read(buffer, response, [self, promise, response]
                        (boost::system::error_code const &ec)
                        {
                            if (ec == boost::asio::error::operation_aborted)
                                return;

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


        self->write(request, [self, promise, receive = std::move(receive_response)]
                (boost::system::error_code const &ec)
                {
                    if (ec == boost::asio::error::operation_aborted)
                        return;

                    if (!ec)
                    {
                        utility::post(self->context_, std::move(receive));
                    }
                    else
                    {
                        auto exception = exception::client{"Failed to post request. " + ec.message()};
                        promise->set_exception(std::make_exception_ptr(std::move(exception)));
                        self->close();
                    }
                }
            );

        return promise->get_future().get();
    }

protected:
    using request_type = boost::beast::http::request<boost::beast::http::string_body>;
    using request_ptr = std::shared_ptr<request_type>;

    using buffer_type = boost::beast::flat_buffer;
    using buffer_ptr = std::shared_ptr<buffer_type>;

    using response_type = boost::beast::http::response<boost::beast::http::string_body>;
    using response_ptr = std::shared_ptr<response_type>;

private:
    boost::asio::io_context &context_;
    core::type::error_handler const &error_handler_;

    virtual void connect(boost::asio::ip::tcp::resolver::results_type const &endpoints,
            std::function<void (boost::system::error_code const &)> on_connect) = 0;
    virtual void close(boost::system::error_code &ec) = 0;
    virtual void write(request_ptr request, std::function<void (boost::system::error_code const &)> on_write) = 0;
    virtual void read(buffer_ptr buffer, response_ptr response,
            std::function<void (boost::system::error_code const &)> on_read) = 0;
};

template <typename TBase, typename TSocket>
class session_base
    : public TBase
{
public:
    template <typename ... TArgs>
    session_base(core::type::error_handler const &error_handler,
            boost::asio::io_context &io_context, TArgs && ... args)
        : session{io_context, error_handler}
        , socket_{io_context, std::forward<TArgs>(args) ... }
    {
    }

protected:
    using base_type = TBase;
    using socket_type = TSocket;

    socket_type & get_socket()
    {
        return socket_;
    }

private:
    socket_type socket_;

    virtual void write(typename base_type::request_ptr request,
            std::function<void (boost::system::error_code const &)> on_write) override final
    {
        boost::beast::http::async_write(socket_, *request,
                [func = std::move(on_write), request]
                (boost::system::error_code const &ec, std::size_t bytes)
                {
                    boost::ignore_unused(bytes);
                    func(ec);
                }
            );
    }

    virtual void read(typename base_type::buffer_ptr buffer, typename base_type::response_ptr response,
            std::function<void (boost::system::error_code const &)> on_read) override final
    {
        boost::beast::http::async_read(socket_, *buffer, *response,
                [func = std::move(on_read), buffer, response]
                (boost::system::error_code const &ec, std::size_t bytes)
                {
                    boost::ignore_unused(bytes);
                    func(ec);
                }
            );
    }
};

class client
    : public std::enable_shared_from_this<client>
{
public:
    client(std::string_view host, std::string_view port, std::size_t workers, core::type::error_handler error_handler)
        : error_handler_{std::move(error_handler)}
        , workers_count_{std::max<int>(1, workers)}
        , context_{workers_count_}
        , work_guard_{boost::asio::make_work_guard(context_)}
    {
        boost::system::error_code ec;
        boost::asio::ip::tcp::resolver resolver{context_};
        endpoints_ = resolver.resolve(host, port, ec);
        if (ec)
            throw exception::client{"Failed to resolve endpoint \"" + std::string{host} + ":" + std::string{port} + "\""};
    }

    virtual ~client() noexcept
    {
        try
        {
            if (stopped())
                return;

            stop();
        }
        catch (std::exception const &e)
        {
            utility::handle_error<exception::client>(error_handler_, e,
                    "[nanorpc::http::detail::client::~client] Failed to done.");
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
                        utility::handle_error<exception::client>(self->error_handler_, std::exception{e},
                                "[nanorpc::client::executor] Failed to execute request. Try again ...");

                        session = self->get_session();
                        response = session->send(std::move(request), dest_location, host);
                    }
                    self->put_session(std::move(session));
                }
                catch (...)
                {
                    if (session)
                        session->close();

                    auto exception = exception::client{"[nanorpc::client::executor] Failed to send data."};
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
                            utility::handle_error<exception::client>(self->error_handler_, e,
                                    "[nanorpc::client::run] Failed to run.");
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
                        utility::handle_error<exception::client>(error_handler_, e,
                                "[nanorpc::client::stop] Failed to stop.");
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

protected:
    using session_ptr = std::shared_ptr<session>;

private:
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

    virtual session_ptr make_session(boost::asio::io_context &io_context,
            core::type::error_handler const &error_handler) = 0;

    session_ptr get_session()
    {
        session_ptr session_item;

        {
            std::lock_guard lock{lock_};
            if (!session_queue_.empty())
            {
                session_item = session_queue_.front();
                session_queue_.pop();
            }
        }

        if (!session_item)
        {
            if (stopped())
                throw exception::client{"Failed to get session. The client was not started."};
            session_item = make_session(context_, error_handler_);
            session_item->connect(endpoints_);
        }

        return session_item;
    }

    void put_session(session_ptr session)
    {
        std::lock_guard lock{lock_};
        session_queue_.emplace(std::move(session));
    }
};

}   // namespace
}   // namespace detail

class client::impl final
    : public detail::client
{
public:
    using detail::client::client;

private:
    virtual session_ptr make_session(boost::asio::io_context &io_context,
            core::type::error_handler const &error_handler) override final
    {
        return std::make_shared<session>(io_context, error_handler);
    }

    class session
        : public detail::session_base<detail::session, boost::asio::ip::tcp::socket>
    {
    public:
        session(boost::asio::io_context &io_context, core::type::error_handler const &error_handler)
            : session_base{error_handler, io_context}
        {
        }

    private:
        virtual void connect(boost::asio::ip::tcp::resolver::results_type const &endpoints,
                std::function<void (boost::system::error_code const &)> on_connect) override final
        {
            boost::asio::async_connect(get_socket(), std::begin(endpoints), std::end(endpoints),
                    [func = std::move(on_connect)] (boost::system::error_code const &ec, auto)
                    {
                        func(ec);
                    }
                );
        }

        virtual void close(boost::system::error_code &ec) override final
        {
            if (!get_socket().is_open())
                return;
            get_socket().shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
            if (ec)
                return;
            get_socket().close(ec);
        }
    };
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

#ifdef NANORPC_WITH_SSL

namespace nanorpc::https
{

class client::impl
    : public http::detail::client
{
public:
    impl(boost::asio::ssl::context ssl_context, std::string_view host, std::string_view port,
            std::size_t workers, core::type::error_handler error_handler)
        : http::detail::client{std::move(host), std::move(port), workers, std::move(error_handler)}
        , ssl_context_{std::move(ssl_context)}
    {
    }

private:
    boost::asio::ssl::context ssl_context_;

    virtual session_ptr make_session(boost::asio::io_context &io_context,
            core::type::error_handler const &error_handler) override final
    {
        return std::make_shared<session>(io_context, ssl_context_, error_handler);
    }

    class session
        : public http::detail::session_base<http::detail::session, boost::asio::ssl::stream<boost::asio::ip::tcp::socket>>
    {
    public:
        session(boost::asio::io_context &io_context, boost::asio::ssl::context &ssl_context,
                core::type::error_handler const &error_handler)
            : session_base{error_handler, io_context, static_cast<boost::asio::ssl::context &>(ssl_context)}
        {
        }

    private:
        virtual void connect(boost::asio::ip::tcp::resolver::results_type const &endpoints,
                std::function<void (boost::system::error_code const &)> on_connect) override final
        {
            boost::asio::async_connect(get_socket().next_layer(), std::begin(endpoints), std::end(endpoints),
                    [this, func = std::move(on_connect)] (boost::system::error_code const &ec, auto)
                    {
                        std::promise<bool> promise;

                        get_socket().async_handshake(boost::asio::ssl::stream_base::client,
                                [&promise] (boost::system::error_code const &ec)
                                {
                                    if (ec)
                                    {
                                        auto exception = exception::client{"Failed to do handshake. " + ec.message()};
                                        promise.set_exception(std::make_exception_ptr(std::move(exception)));
                                        return;
                                    }

                                    promise.set_value(true);
                                });

                        promise.get_future().get();
                        func(ec);
                    }
                );
        }

        virtual void close(boost::system::error_code &ec) override final
        {
            boost::ignore_unused(ec);
            if (!get_socket().next_layer().is_open())
                return;
            get_socket().async_shutdown([] (boost::system::error_code const &ec)
                    { boost::ignore_unused(ec); } );
        }
    };
};

client::client(boost::asio::ssl::context context, std::string_view host, std::string_view port, std::size_t workers,
            std::string_view location, core::type::error_handler error_handler)
    : impl_{std::make_shared<impl>(std::move(context), std::move(host), std::move(port), workers, std::move(error_handler))}
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

}   // namespace nanorpc::https

#endif  // !NANORPC_WITH_SSL
