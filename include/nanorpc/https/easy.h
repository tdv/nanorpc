//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTPS_EASY_H__
#define __NANO_RPC_HTTPS_EASY_H__

// NANORPC
#include "nanorpc/core/detail/config.h"
#if !defined(NANORPC_PURE_CORE) && defined(NANORPC_WITH_SSL)

// STD
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

// BOOST
#include <boost/asio/ssl/context.hpp>

// NANORPC
#include "nanorpc/core/client.h"
#include "nanorpc/core/server.h"
#include "nanorpc/core/type.h"
#include "nanorpc/https/client.h"
#include "nanorpc/https/server.h"
#include "nanorpc/packer/plain_text.h"

namespace nanorpc::https::easy
{

inline core::client<packer::plain_text>
make_client(boost::asio::ssl::context context, std::string_view host, std::string_view port,
        std::size_t workers, std::string_view location)
{
    auto https_client = std::make_shared<client>(std::move(context), std::move(host), std::move(port),
            workers, std::move(location));
    https_client->run();
    auto executor_proxy = [executor = https_client->get_executor(), https_client]
            (core::type::buffer request)
            {
                return executor(std::move(request));
            };
    return {std::move(executor_proxy)};
}

template <typename ... T>
inline server make_server(boost::asio::ssl::context context, std::string_view address, std::string_view port,
        std::size_t workers, std::string_view location, std::pair<char const *, T> const & ... handlers)
{
    auto core_server = std::make_shared<core::server<packer::plain_text>>();
    (core_server->handle(handlers.first, handlers.second), ... );

    auto executor = [srv = std::move(core_server)]
            (core::type::buffer request)
            {
                return srv->execute(std::move(request));
            };

    core::type::executor_map executors;
    executors.emplace(std::move(location), std::move(executor));

    server https_server(std::move(context), std::move(address), std::move(port), workers, std::move(executors));
    https_server.run();

    return https_server;
}


}   // namespace nanorpc::https::easy

#endif  // !NANORPC_WITH_SSL
#endif  // !__NANO_RPC_HTTPS_EASY_H__
