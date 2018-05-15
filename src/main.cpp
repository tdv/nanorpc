#include <iostream>

#include <nanorpc/packer/plain_text.h>
#include <nanorpc/core/server.h>
#include <nanorpc/core/client.h>

int main()
{
    try
    {
        nanorpc::core::server<nanorpc::packer::plain_text> server;
        server.handle("test", [] (std::string const &s) { std::cout << "Param: " << s << std::endl; } );

        auto sender = [&] (nanorpc::core::type::buffer request)
            {
                return server.execute(std::move(request));
            };

        nanorpc::core::client<nanorpc::packer::plain_text> client(std::move(sender));
        client.call("test", std::string{"test"});
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
