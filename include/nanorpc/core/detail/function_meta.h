//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_CORE_DETAIL_FUNCTION_META_H__
#define __NANO_RPC_CORE_DETAIL_FUNCTION_META_H__

// STD
#include <functional>
#include <tuple>
#include <type_traits>

namespace nanorpc::core::detail
{

template <typename>
struct function_meta;

template <typename R, typename ... T>
struct function_meta<std::function<R (T ... )>>
{
    using return_type = std::decay_t<R>;
    using arguments_tuple_type = std::tuple<std::decay_t<T> ... >;
};

}   // namespace nanorpc::core::detail


#endif  // !__NANO_RPC_CORE_DETAIL_FUNCTION_META_H__
