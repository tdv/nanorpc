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
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace nanorpc::core::type
{

using id = std::size_t;
using buffer = std::vector<char>;
using executor = std::function<buffer (buffer)>;
using executor_map = std::map<std::string, executor>;
using error_handler = std::function<void (std::exception_ptr)>;

}   // namespace nanorpc::core::type


#endif  // !__NANO_RPC_CORE_TYPE_H__
