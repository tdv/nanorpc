//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_PACKER_DETAIL_TO_TUPLE_H__
#define __NANO_RPC_PACKER_DETAIL_TO_TUPLE_H__

// STD
#include <tuple>
#include <type_traits>

// BOOST
#include <boost/preprocessor.hpp>

// NANORPC
#include "nanorpc/packer/detail/traits.h"

namespace nanorpc::packer::detail
{

template <typename T>
auto to_tuple(T &&value)
{
    using type = std::decay_t<T>;

#define NANORPC_TO_TUPLE_LIMIT_FIELDS 64 // you can try to use BOOST_PP_LIMIT_REPEAT

#define NANORPC_TO_TUPLE_DUMMY_TYPE_N(_, n, data) \
    BOOST_PP_COMMA_IF(n) data

#define NANORPC_TO_TUPLE_PARAM_N(_, n, data) \
    BOOST_PP_COMMA_IF(n) data ## n

#define NANORPC_TO_TUPLE_ITEM_N(_, n, __) \
    if constexpr (is_braces_constructible_v<type, \
    BOOST_PP_REPEAT_FROM_TO(0, BOOST_PP_SUB(NANORPC_TO_TUPLE_LIMIT_FIELDS, n), NANORPC_TO_TUPLE_DUMMY_TYPE_N, dummy_type) \
    >) { auto &&[ \
    BOOST_PP_REPEAT_FROM_TO(0, BOOST_PP_SUB(NANORPC_TO_TUPLE_LIMIT_FIELDS, n), NANORPC_TO_TUPLE_PARAM_N, f) \
    ] = value; return std::make_tuple( \
    BOOST_PP_REPEAT_FROM_TO(0, BOOST_PP_SUB(NANORPC_TO_TUPLE_LIMIT_FIELDS, n), NANORPC_TO_TUPLE_PARAM_N, f) \
    ); } else

#define NANORPC_TO_TUPLE_ITEMS(n) \
    BOOST_PP_REPEAT_FROM_TO(0, n, NANORPC_TO_TUPLE_ITEM_N, nil)

    NANORPC_TO_TUPLE_ITEMS(NANORPC_TO_TUPLE_LIMIT_FIELDS)
    {
        return std::make_tuple();
    }

#undef NANORPC_TO_TUPLE_ITEMS
#undef NANORPC_TO_TUPLE_ITEM_N
#undef NANORPC_TO_TUPLE_PARAM_N
#undef NANORPC_TO_TUPLE_DUMMY_TYPE_N
#undef NANORPC_TO_TUPLE_LIMIT_FIELDS
}

}   // namespace nanorpc::packer::detail

#endif  // !__NANO_RPC_PACKER_DETAIL_TO_TUPLE_H__
