#include <iostream>

#include <nanorpc/packer/plain_text.h>
#include <nanorpc/core/server.h>
#include <nanorpc/http/server.h>
#include <nanorpc/http/easy.h>

int main()
{
    try
    {
        auto server = nanorpc::http::easy::make_server("0.0.0.0", "55555", 8, "/api/",
                std::pair{"test", [] (std::string const &s) { std::cout << "Param: " << s << std::endl; return std::string{"tested"}; } }
            );

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
