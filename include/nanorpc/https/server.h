//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTPS_SERVER_H__
#define __NANO_RPC_HTTPS_SERVER_H__

// STD
#include <cstdint>
#include <memory>
#include <string>

// BOOST
#include <boost/asio/ssl/context.hpp>

// NANORPC
#include "nanorpc/core/exception.h"
#include <nanorpc/core/type.h>

namespace nanorpc::http::detail { class server_impl; }

namespace nanorpc::https
{

NANORPC_EXCEPTION_DECL_WITH_NAMESPACE(exception, server, core::exception::server)

class server final
{
public:
    server(boost::asio::ssl::context context, std::string_view address, std::string_view port,
           std::size_t workers, core::type::executor_map executors,
           core::type::error_handler error_handler = core::exception::default_error_handler);

    ~server() noexcept;
    void run();
    void stop();
    bool stopped() const noexcept;

private:
    std::shared_ptr<http::detail::server_impl> impl_;
};

}   // namespace nanorpc::https


#endif  // !__NANO_RPC_HTTPS_SERVER_H__
