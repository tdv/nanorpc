//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_CORE_TYPE_H__
#define __NANO_RPC_CORE_TYPE_H__

// STD
#include <cstdint>
#include <vector>

namespace nanorpc::core::type
{

using id = std::size_t;
using buffer = std::vector<char>;

}   // namespace nanorpc::core::type


#endif  // !__NANO_RPC_CORE_TYPE_H__
