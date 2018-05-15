//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_CORE_CLIENT_H__
#define __NANO_RPC_CORE_CLIENT_H__

// STD
#include <any>
#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>

// NANORPC
#include "nanorpc/core/type.h"
#include "nanorpc/version/core.h"

namespace nanorpc::core
{

template <typename TPacker>
class client final
{
private:
    class result;

public:
    using sender_type = std::function<type::buffer (type::buffer)>;

    client(sender_type sender)
        : sender_{std::move(sender)}
    {
    }

    template <typename ... TArgs>
    result call(std::string_view name, TArgs && ... args)
    {
        return call(std::hash<std::string_view>{}(name), std::forward<TArgs>(args) ... );
    }

    template <typename ... TArgs>
    result call(type::id id, TArgs && ... args)
    {
        auto meta = std::make_tuple(version::core::protocol::value, std::move(id));
        auto data = std::make_tuple(std::forward<TArgs>(args) ... );

        packer_type packer;
        auto request = packer
                .pack(meta)
                .pack(data)
                .to_buffer();

        auto buffer = sender_(std::move(request));
        auto response = packer.from_buffer(std::move(buffer));
        decltype(meta) response_meta;
        response = response.unpack(response_meta);
        if (meta != response_meta)
            throw std::runtime_error{"[" + std::string{__func__ } + "] The meta in the response is bad."};

        return {std::move(response)};
    }

private:
    using packer_type = TPacker;
    using deserializer_type = typename packer_type::deserializer_type;

    sender_type sender_;

    class result final
    {
    public:
        result(result &&) noexcept = default;
        result& operator = (result &&) noexcept = default;
        ~result() noexcept = default;

        template <typename T>
        T as() const
        {
            if (!value_ && !deserializer_)
                throw std::runtime_error{"[" + std::string{__func__ } + "] No data."};

            using Type = std::decay_t<T>;

            if (!value_)
            {
                 if (!deserializer_)
                     throw std::runtime_error{"[" + std::string{__func__ } + "] No data."};

                 Type data{};
                 deserializer_->unpack(data);

                 value_ = std::move(data);
                 deserializer_.reset();
            }

            return std::any_cast<Type>(*value_);
        }

        template <typename T>
        operator T () const
        {
            return as<T>();
        }

    private:
        template <typename>
        friend class client;

        mutable std::optional<deserializer_type> deserializer_;
        mutable std::optional<std::any> value_;

        result(deserializer_type deserializer)
            : deserializer_{std::move(deserializer)}
        {
        }

        result(result const &) = delete;
        result& operator = (result const &) = delete;
    };
};

}   // namespace nanorpc::core

#endif  // !__NANO_RPC_CORE_CLIENT_H__
