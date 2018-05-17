//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTP_DETAIL_SESSION_H__
#define __NANO_RPC_HTTP_DETAIL_SESSION_H__

// STD
#include <memory>

// BOOST
#include <boost/asio.hpp>
#include <boost/beast.hpp>

// THIS
#include "data.h"

namespace nanorpc::http::detail
{

class session final
    : public std::enable_shared_from_this<session>
{
public:
    session(boost::asio::ip::tcp::socket socket, executor_data const &data);
    void run() noexcept;
    void post_data(core::type::buffer buffer);

private:
    using request_type = boost::beast::http::request<boost::beast::http::string_body>;
    using response_type = boost::beast::http::response<boost::beast::http::string_body>;

    executor_data data_;

    boost::beast::flat_buffer buffer_;
    request_type request_;
    boost::asio::ip::tcp::socket socket_;
    boost::asio::strand<boost::asio::io_context::executor_type> strand_;

    void read();
    void on_read(boost::system::error_code const &ec) noexcept;
    void close();
    void handle_request(request_type req);
    void on_write(boost::system::error_code const &, std::shared_ptr<response_type>);

};

}   // namespace nanorpc::http::detail

#endif  // !__NANO_RPC_HTTP_DETAIL_SESSION_H__
