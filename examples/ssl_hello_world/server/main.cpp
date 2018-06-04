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

        auto server = nanorpc::https::easy::make_server(std::move(context), "0.0.0.0", "55555", 8, "/api/",
                std::pair{"test", [] (std::string const &s) { return "Tested: " + s; } }
            );

        std::cout << "Press Enter for quit." << std::endl;

        std::cin.get();
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << nanorpc::core::exception::to_string(e) << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
