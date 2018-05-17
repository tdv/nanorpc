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

// THIS
#include "data.h"

// TODO: remove it
#include <iostream>

namespace nanorpc::http::detail::utility
{

template <typename T>
inline std::enable_if_t<std::is_invocable_v<T>, void>
post(boost::asio::io_context &context, T func) noexcept
{
    try
    {
        boost::asio::post(context,
                [callable = std::move(func)]
                {
                    try
                    {
                        callable();
                    }
                    catch (std::exception const &e)
                    {
                        // TODO:
                        std::cerr << "Failed to call method. Error: " << e.what() <<  std::endl;
                    }
                }
            );
    }
    catch (std::exception const &e)
    {
        // TODO:
        std::cerr << "Failed to post task. Error: " << e.what() << std::endl;
    }
}

inline void handle_error(executor_data const &data, std::exception_ptr exception) noexcept
{
    try
    {
        if (data.error_handler_)
            data.error_handler_(std::move(exception));
    }
    catch (...)
    {
    }

    // TODO:
    try
    {
        std::rethrow_exception(exception);
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
    }
}

inline void handle_error(executor_data const &data, std::string const &message) noexcept
{
    try
    {
        handle_error(data, std::make_exception_ptr(std::runtime_error{message}));
    }
    catch (...)
    {
    }
}

}   // namespace nanorpc::http::detail::utility

#endif  // !__NANO_RPC_HTTP_DETAIL_UTILITY_H__
