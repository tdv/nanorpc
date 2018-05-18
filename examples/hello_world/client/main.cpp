#include <iostream>

#include <nanorpc/packer/plain_text.h>
#include <nanorpc/core/client.h>
#include <nanorpc/http/client.h>

#include <thread>
#include <chrono>

int main()
{
    try
    {
        nanorpc::http::client http_client{"localhost", "55555", 8, "/api/"};
        http_client.run();

        std::cout << "Client started." << std::endl;

        nanorpc::core::client<nanorpc::packer::plain_text> client(http_client.get_executor());

        std::string from_server = client.call("test", std::string{"test"});
        std::cout << "Response from server: " << from_server << std::endl;

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
