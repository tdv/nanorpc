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

// NANORPC
#include "nanorpc/core/detail/config.h"
#include "nanorpc/packer/detail/traits.h"

#ifndef NANORPC_PURE_CORE

// BOOST
#include <boost/preprocessor.hpp>

#endif  // !NANORPC_PURE_CORE

namespace nanorpc::packer::detail
{

#ifndef NANORPC_PURE_CORE

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

#else

template <typename T>
auto to_tuple(T &&value)
{
    using type = std::decay_t<T>;

    if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3, f4, f5, f6, f7, f8, f9, f10] = value;
        return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9, f10);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3, f4, f5, f6, f7, f8, f9] = value;
        return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8, f9);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3, f4, f5, f6, f7, f8] = value;
        return std::make_tuple(f1, f2, f3, f4, f5, f6, f7, f8);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3, f4, f5, f6, f7] = value;
        return std::make_tuple(f1, f2, f3, f4, f5, f6, f7);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3, f4, f5, f6] = value;
        return std::make_tuple(f1, f2, f3, f4, f5, f6);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3, f4, f5] = value;
        return std::make_tuple(f1, f2, f3, f4, f5);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3, f4] = value;
        return std::make_tuple(f1, f2, f3, f4);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2, f3] = value;
        return std::make_tuple(f1, f2, f3);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type, dummy_type>)
    {
        auto &&[f1, f2] = value;
        return std::make_tuple(f1, f2);
    }
    else if constexpr (is_braces_constructible_v<type, dummy_type>)
    {
        auto &&[f1] = value;
        return std::make_tuple(f1);
    }
    else
    {
        return std::make_tuple();
    }
}

#endif  // !NANORPC_PURE_CORE

}   // namespace nanorpc::packer::detail

#endif  // !__NANO_RPC_PACKER_DETAIL_TO_TUPLE_H__
