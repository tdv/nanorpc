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
#include <nanorpc/core/client.h>
#include <nanorpc/core/exception.h>
#include <nanorpc/core/server.h>
#include <nanorpc/packer/plain_text.h>

int main()
{
    try
    {
        nanorpc::core::server<nanorpc::packer::plain_text> server;
        server.handle("test", [] (std::string const &s)
            {
                std::cout << "Server. Method \"test\". Input: " << s << std::endl;
                return "echo \"" + s + "\"";
            } );

        auto executor = [srv = std::move(server)]
            (nanorpc::core::type::buffer request) mutable
            {
                std::cout << "Dump. Request: '"
                          << std::string{begin(request), end(request)}
                          << "'" << std::endl;

                auto response = srv.execute(std::move(request));

                std::cout << "Dump. Response: '"
                          << std::string{begin(response), end(response)}
                          << "'" << std::endl;

                return response;
            };

        nanorpc::core::client<nanorpc::packer::plain_text> client{std::move(executor)};

        std::string response = client.call("test", "hello world !!!");
        std::cout << "Client. Method \"test\" Output: " << response << std::endl;
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << nanorpc::core::exception::to_string(e) << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
