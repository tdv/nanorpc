//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTPS_CLIENT_H__
#define __NANO_RPC_HTTPS_CLIENT_H__

// NANORPC
#include "nanorpc/core/detail/config.h"
#if !defined(NANORPC_PURE_CORE) && defined(NANORPC_WITH_SSL)

// STD
#include <cstdint>
#include <memory>
#include <string>

// BOOST
#include <boost/asio/ssl/context.hpp>

// NANORPC
#include "nanorpc/core/exception.h"
#include "nanorpc/core/type.h"

namespace nanorpc::https
{

NANORPC_EXCEPTION_DECL_WITH_NAMESPACE(exception, client, core::exception::client)

class client final
{
public:
    client(boost::asio::ssl::context context, std::string_view host, std::string_view port,
            std::size_t workers, std::string_view location,
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

}   // namespace nanorpc::https

#endif  // !NANORPC_WITH_SSL
#endif  // !__NANO_RPC_HTTPS_CLIENT_H__
