//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __NANO_RPC_HTTP_DETAIL_UTILITY_H__
#define __NANO_RPC_HTTP_DETAIL_UTILITY_H__

// STD
#include <stdexcept>
#include <string>
#include <type_traits>
#include <utility>

// BOOST
#include <boost/asio.hpp>
#include <boost/core/ignore_unused.hpp>

// NANORPC
#include "nanorpc/core/exception.h"
#include "nanorpc/core/type.h"

namespace nanorpc::http::detail::utility
{

template <typename T>
inline std::enable_if_t<std::is_invocable_v<T>, void>
post(boost::asio::io_context &context, T func, core::type::error_handler error_handler = {}) noexcept
{
    try
    {
        boost::asio::post(context,
                [callable = std::move(func), error_handler]
                {
                    try
                    {
                        callable();
                    }
                    catch (std::exception const &e)
                    {
                        try
                        {
                            if (error_handler)
                                error_handler(std::make_exception_ptr(e));
                        }
                        catch (...)
                        {
                        }
                    }
                }
            );
    }
    catch (std::exception const &e)
    {
        try
        {
            if (error_handler)
                error_handler(std::make_exception_ptr(e));
        }
        catch (...)
        {
        }
    }
}

template <typename TEx, typename ... TMsg>
inline void handle_error(core::type::error_handler const &error_handler, TMsg const & ... message_items) noexcept
{
    try
    {
        if (!error_handler)
            return;

        std::string message;
        boost::ignore_unused((message += ... += std::string{message_items}));
        auto exception = std::make_exception_ptr(TEx{std::move(message)});
        error_handler(std::move(exception));
    }
    catch (...)
    {
    }
}

template <typename TEx, typename ... TMsg>
inline void handle_error(core::type::error_handler const &error_handler, std::exception_ptr nested, TMsg const & ... message_items) noexcept
{
    handle_error<TEx>(error_handler, message_items ... );

    if (!nested)
        return;

    try
    {
        try
        {
            std::rethrow_exception(nested);
        }
        catch (std::exception const &e)
        {
            handle_error<TEx>(error_handler, e.what());
        }

        std::rethrow_if_nested(nested);
    }
    catch (...)
    {
        handle_error<core::exception::nanorpc>(error_handler, std::current_exception());
    }
}

template <typename TEx, typename ... TMsg>
inline void handle_error(core::type::error_handler const &error_handler, std::exception const &e, TMsg const & ... message_items) noexcept
{
    handle_error<TEx>(error_handler, std::make_exception_ptr(e), message_items ... );
}

}   // namespace nanorpc::http::detail::utility

#endif  // !__NANO_RPC_HTTP_DETAIL_UTILITY_H__
