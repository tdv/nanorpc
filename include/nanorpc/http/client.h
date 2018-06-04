//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTP_CLIENT_H__
#define __NANO_RPC_HTTP_CLIENT_H__

// NANORPC
#include "nanorpc/core/detail/config.h"
#ifndef NANORPC_PURE_CORE

// STD
#include <cstdint>
#include <memory>
#include <string>

// NANORPC
#include "nanorpc/core/exception.h"
#include "nanorpc/core/type.h"

namespace nanorpc::http
{

NANORPC_EXCEPTION_DECL_WITH_NAMESPACE(exception, client, core::exception::client)

class client final
{
public:
    client(std::string_view host, std::string_view port, std::size_t workers, std::string_view location,
            core::type::error_handler error_handler = core::exception::default_error_handler);

    ~client() noexcept;
    void run();
    void stop();
    bool stopped() const noexcept;

    core::type::executor const& get_executor() const;

private:
    class impl;
    std::shared_ptr<impl> impl_;
};

}   // namespace nanorpc::http

#endif  // !NANORPC_PURE_CORE
#endif  // !__NANO_RPC_HTTP_CLIENT_H__
