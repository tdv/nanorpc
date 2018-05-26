//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTP_DETAIL_CONSTANTS_H__
#define __NANO_RPC_HTTP_DETAIL_CONSTANTS_H__

namespace nanorpc::http::detail::constants
{

inline constexpr auto server_name = "Nano RPC server";
inline constexpr auto user_agent_name = "Nano RPC user agent";
inline constexpr auto content_type = "text/html";

inline constexpr auto http_version = 11;

}   // namespace nanorpc::http::detail::constants

#endif  // !__NANO_RPC_HTTP_DETAIL_CONSTANTS_H__
