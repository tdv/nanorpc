//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTP_EASY_H__
#define __NANO_RPC_HTTP_EASY_H__

// STD
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

// NANORPC
#include "nanorpc/core/client.h"
#include "nanorpc/http/client.h"
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


}   // namespace nanorpc::http::easy


#endif  // !__NANO_RPC_HTTP_EASY_H__
