//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTP_EASY_H__
#define __NANO_RPC_HTTP_EASY_H__

// NANORPC
#include "nanorpc/core/detail/config.h"
#ifndef NANORPC_PURE_CORE

// STD
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

// NANORPC
#include "nanorpc/core/client.h"
#include "nanorpc/core/server.h"
#include "nanorpc/core/type.h"
#include "nanorpc/http/client.h"
#include "nanorpc/http/server.h"
#include "nanorpc/packer/plain_text.h"

namespace nanorpc::http::easy
{

inline core::client<packer::plain_text>
make_client(std::string_view host, std::string_view port, std::size_t workers, std::string_view location)
{
    auto http_client = std::make_shared<client>(std::move(host), std::move(port), workers, std::move(location));
    http_client->run();
    auto executor_proxy = [executor = http_client->get_executor(), http_client]
            (core::type::buffer request)
            {
                return executor(std::move(request));
            };
    return {std::move(executor_proxy)};
}

template <typename ... T>
inline server make_server(std::string_view address, std::string_view port, std::size_t workers,
                          std::string_view location, std::pair<char const *, T> const & ... handlers)
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

    server http_server(std::move(address), std::move(port), workers, std::move(executors));
    http_server.run();

    return http_server;
}


}   // namespace nanorpc::http::easy

#endif  // !NANORPC_PURE_CORE
#endif  // !__NANO_RPC_HTTP_EASY_H__
