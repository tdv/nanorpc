#include <iostream>

#include <nanorpc/packer/plain_text.h>
#include <nanorpc/core/server.h>
#include <nanorpc/http/server.h>

int main()
{
    try
    {
        nanorpc::core::server<nanorpc::packer::plain_text> server;
        server.handle("test", [] (std::string const &s) { std::cout << "Param: " << s << std::endl; return std::string{"tested"}; } );

        auto sender = [&] (nanorpc::core::type::buffer request)
            {
                return server.execute(std::move(request));
            };

        nanorpc::core::type::executor_map em{
                {"/api/", sender}
            };

        nanorpc::http::server http_server{"127.0.0.1", "55555", 8, em};
        http_server.run();

        std::cout << "Server started." << std::endl;
        std::cout << "Press Enter for quit." << std::endl;

        std::cin.get();
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
