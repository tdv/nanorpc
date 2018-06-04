//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     06.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

// STD
#include <cstdlib>
#include <iostream>

// NANORPC
#include <nanorpc/https/easy.h>

// THIS
#include "common/ssl_context.h"

int main()
{
    try
    {
        auto context = prepare_ssl_context("cert.pem", "key.pem", "dh.pem");

        auto client = nanorpc::https::easy::make_client(std::move(context), "localhost", "55555", 8, "/api/");

        std::string result = client.call("test", std::string{"test"});
        std::cout << "Response from server: " << result << std::endl;
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << nanorpc::core::exception::to_string(e) << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
