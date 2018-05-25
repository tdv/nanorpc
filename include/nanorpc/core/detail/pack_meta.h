//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_CORE_DETAIL_PACK_META_H__
#define __NANO_RPC_CORE_DETAIL_PACK_META_H__

// STD
#include <cstdint>

namespace nanorpc::core::detail::pack::meta
{

enum class type : std::uint32_t
{
    unknown,
    request,
    response,
};

enum class status : std::uint32_t
{
    fail,
    good
};

}   // namespace nanorpc::core::detail::pack::meta


#endif  // !__NANO_RPC_CORE_DETAIL_PACK_META_H__
