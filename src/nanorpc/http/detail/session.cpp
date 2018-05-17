//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

// THIS
#include "session.h"
#include "utility.h"

namespace nanorpc::http::detail
{

session::session(boost::asio::ip::tcp::socket socket, executor_data const &data)
    : data_{data}
    , socket_{std::move(socket)}
    , strand_{socket_.get_executor()}
{
}

void session::run() noexcept
{
    utility::post(socket_.get_executor().context(), [self = shared_from_this()] { self->read(); } );
}

void session::post_data(core::type::buffer buffer)
{
    (void)buffer;
}

void session::read()
{
    request_ = {};

    boost::beast::http::async_read(socket_, buffer_, request_,
            boost::asio::bind_executor(strand_,
                    std::bind(&session::on_read, shared_from_this(), std::placeholders::_1)
                )
        );
}

void session::on_read(boost::system::error_code const &ec) noexcept
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
        utility::handle_error(data_, "Failed to handle request. Error: " + std::string{e.what()});
        close();
    }
}

void session::close()
{
    if (!socket_.is_open())
        return;

    boost::system::error_code ec;
    socket_.shutdown(boost::asio::ip::tcp::socket::shutdown_send, ec);
    if (ec)
        utility::handle_error(data_, "Failed to close socket. Message: " + ec.message());
}

void session::handle_request(request_type req)
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
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body().assign({begin(buffer), end(buffer)});
            res.prepare_payload();
            return res;
        };

    auto const not_found = [&req, &target]
        {
            response_type res{boost::beast::http::status::not_found, req.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "The resource '" + target + "' was not found.";
            res.prepare_payload();
            return res;
        };

    auto const server_error =
        [&req](boost::beast::string_view what)
        {
            response_type res{boost::beast::http::status::internal_server_error, req.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = "An error occurred: '" + what.to_string() + "'";
            res.prepare_payload();
            return res;
        };

    auto const bad_request =
    [&req](boost::beast::string_view why)
        {
            response_type res{boost::beast::http::status::bad_request, req.version()};
            res.set(boost::beast::http::field::server, BOOST_BEAST_VERSION_STRING);
            res.set(boost::beast::http::field::content_type, "text/html");
            res.keep_alive(req.keep_alive());
            res.body() = why.to_string();
            res.prepare_payload();
            return res;
        };

    auto const iter = data_.executors_.find(target);
    if (iter == end(data_.executors_))
    {
        utility::handle_error(data_, "Not found. " + target);
        reply(not_found());
        return;
    }



    auto &executor = iter->second;
    if (!executor)
    {
        utility::handle_error(data_, "Empty exicutor.");
        reply(server_error("Empty exicutor."));
        return;
    }


    auto const &content = req.body();
    if (content.empty())
    {
        utility::handle_error(data_, "No content.");
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
        reply(server_error("Empty exicutor."));
        utility::handle_error(data_, e.what());
        return;
    }
}

void session::on_write(boost::system::error_code const &, std::shared_ptr<response_type>)
{
    //close();
}

}   // namespace nanorpc::http::detail
