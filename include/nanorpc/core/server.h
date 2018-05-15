//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_CORE_SERVER_H__
#define __NANO_RPC_CORE_SERVER_H__

// STD
#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

// BOOST
#include <boost/core/ignore_unused.hpp>

// NANORPC
#include "nanorpc/core/detail/function_meta.h"
#include "nanorpc/core/type.h"
#include "nanorpc/version/core.h"

namespace nanorpc::core
{

template <typename TPacker>
class server final
{
public:
    template <typename TFunc>
    void handle(std::string_view name, TFunc func)
    {
        handle(std::hash<std::string_view>{}(name), std::move(func));
    }

    template <typename TFunc>
    void handle(type::id id, TFunc func)
    {
        if (handlers_.find(id) != end(handlers_))
        {
            throw std::invalid_argument{"[" + std::string{__func__ } + "] Failed to add handler. "
                    "The id \"" + std::to_string(id) + "\" already exists."};
        }

        auto wrapper = [f = std::move(func)] (deserializer_type &request, serializer_type &response)
            {
                std::function func{std::move(f)};
                using function_meta = detail::function_meta<decltype(func)>;
                using arguments_tuple_type = typename function_meta::arguments_tuple_type;
                arguments_tuple_type data;
                request.unpack(data);
                apply(std::move(func), std::move(data), response);
            };

        handlers_.emplace(std::move(id), std::move(wrapper));
    }

    type::buffer execute(type::buffer buffer)
    {
        if (handlers_.empty())
            throw std::runtime_error{"[" + std::string{__func__ } + "] No handlers."};

        packer_type packer;

        using meta_type = std::tuple<version::core::protocol::value_type, type::id>;
        meta_type meta;
        auto request = packer.from_buffer(std::move(buffer)).unpack(meta);

        auto const protocol_version = std::get<0>(meta);
        if (protocol_version != version::core::protocol::value)
        {
            throw std::runtime_error{"[" + std::string{__func__ } + "] Failed to process data. "
                    "Protocol \"" + std::to_string(protocol_version) + "\" not supported."};
        }

        auto const function_id = std::get<1>(meta);
        auto const iter = handlers_.find(function_id);
        if (iter == end(handlers_))
        {
            throw std::runtime_error{"[" + std::string{__func__ } + "] Function with id "
                    "\"" + std::to_string(function_id) + "\" not found."};
        }

        auto response = packer.pack(meta);
        iter->second(request, response);

        return response.to_buffer();
    }

private:
    using packer_type = TPacker;
    using serializer_type = typename packer_type::serializer_type;
    using deserializer_type = typename packer_type::deserializer_type;
    using handler_type = std::function<void (deserializer_type &, serializer_type &)>;
    using handlers_type = std::map<type::id, handler_type>;

    handlers_type handlers_;

    template <typename TFunc, typename TArgs>
    static
    std::enable_if_t<!std::is_same_v<std::decay_t<decltype(std::apply(std::declval<TFunc>(), std::declval<TArgs>()))>, void>, void>
    apply(TFunc func, TArgs args, serializer_type &serializer)
    {
        auto data = std::apply(std::move(func), std::move(args));
        serializer = serializer.pack(data);
    }

    template <typename TFunc, typename TArgs>
    static
    std::enable_if_t<std::is_same_v<std::decay_t<decltype(std::apply(std::declval<TFunc>(), std::declval<TArgs>()))>, void>, void>
    apply(TFunc func, TArgs args, serializer_type &serializer)
    {
        boost::ignore_unused(serializer);
        std::apply(std::move(func), std::move(args));
    }
};

}   // namespace nanorpc::core

#endif  // !__NANO_RPC_CORE_SERVER_H__
