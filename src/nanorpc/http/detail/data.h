//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTP_DETAIL_DATA_H__
#define __NANO_RPC_HTTP_DETAIL_DATA_H__

// NANORPC
#include "nanorpc/core/type.h"

namespace nanorpc::http::detail
{

struct executor_data final
{
    core::type::executor_map executors_;
    core::type::error_handler error_handler_;
};

}   // namespace nanorpc::http::detail

#endif  // !__NANO_RPC_HTTP_DETAIL_DATA_H__
