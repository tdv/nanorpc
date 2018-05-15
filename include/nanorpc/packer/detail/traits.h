//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_PACKER_DETAIL_TRAITS_H__
#define __NANO_RPC_PACKER_DETAIL_TRAITS_H__

// STD
#include <cstdint>
#include <tuple>
#include <type_traits>

namespace nanorpc::packer::detail
{
inline namespace traits
{

template <typename T>
constexpr std::true_type is_iterable(T const &,
        typename T::iterator = typename std::decay<T>::type{}.begin(),
        typename T::iterator = typename std::decay<T>::type{}.end(),
        typename T::const_iterator = typename std::decay<T>::type{}.cbegin(),
        typename T::const_iterator = typename std::decay<T>::type{}.cend(),
        typename T::value_type = * typename std::decay<T>::type{}.begin()
    ) noexcept;

constexpr std::false_type is_iterable(...) noexcept;

template <typename T>
constexpr bool is_iterable_v = std::decay_t<decltype(is_iterable(*static_cast<T const *>(nullptr)))>::value;

template <typename ... T>
constexpr std::true_type is_tuple(std::tuple<T ... > const &) noexcept;

constexpr std::false_type is_tuple(...) noexcept;

template <typename T>
constexpr bool is_tuple_v = std::decay_t<decltype(is_tuple(*static_cast<T const *>(nullptr)))>::value;

struct dummy_type final
{
    template <typename T>
    constexpr operator T () noexcept
    {
        return *static_cast<T const *>(nullptr);
    }
};

template <typename T, typename ... TArgs>
constexpr decltype(void(T{std::declval<TArgs>() ... }), std::declval<std::true_type>())
is_braces_constructible(std::size_t) noexcept;

template <typename, typename ... >
constexpr std::false_type is_braces_constructible(...) noexcept;

template <typename T, typename ... TArgs>
constexpr bool is_braces_constructible_v = std::decay_t<decltype(is_braces_constructible<T, TArgs ... >(0))>::value;

}   // inline namespace traits
}   // namespace nanorpc::packer::detail

#endif  // !__NANO_RPC_PACKER_DETAIL_TRAITS_H__
