//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_CORE_EXCEPTION_H__
#define __NANO_RPC_CORE_EXCEPTION_H__

// STD
#include <stdexcept>

#define NANORPC_EXCEPTION_DECL(class_, base_) \
    class class_ \
        : public base_ \
    { \
    public: \
        using base_type = base_; \
        using base_type :: base_type; \
    };

#define NANORPC_EXCEPTION_DECL_WITH_NAMESPACE(namespace_, class_, base_) \
    namespace namespace_ \
    { \
        NANORPC_EXCEPTION_DECL(class_, base_) \
    }

namespace nanorpc::core::exception
{

NANORPC_EXCEPTION_DECL(exception, std::runtime_error)
NANORPC_EXCEPTION_DECL(logic, exception)
NANORPC_EXCEPTION_DECL(transport, exception)
NANORPC_EXCEPTION_DECL(client, transport)
NANORPC_EXCEPTION_DECL(server, transport)

}   // namespace nanorpc::core::exception


#endif  // !__NANO_RPC_CORE_EXCEPTION_H__
