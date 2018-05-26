//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

// STD
#include <cstdlib>
#include <iostream>
#include <mutex>

// NANORPC
#include <nanorpc/http/easy.h>

// THIS
#include "common/data.h"

int main()
{
    try
    {
        std::mutex mutex;
        data::employees employees;

        auto server = nanorpc::http::easy::make_server("0.0.0.0", "55555", 8, "/api/",
                std::pair{"create", [&]
                    (std::string const &id, data::employee const &employee)
                    {
                        std::lock_guard loxk{mutex};
                        if (employees.find(id) != std::end(employees))
                            throw std::invalid_argument{"Employee with id \"" + id + "\" already exists."};
                        employees.emplace(id, employee);
                        return id;
                    } },
                std::pair{"read", [&]
                    (std::string const &id)
                    {
                        std::lock_guard loxk{mutex};
                        auto const iter = employees.find(id);
                        if (iter == std::end(employees))
                            throw std::invalid_argument{"Employee with id \"" + id + "\" not found."};
                        return iter->second;
                    } },
                std::pair{"update", [&]
                    (std::string const &id, data::employee const &employee)
                    {
                        std::lock_guard loxk{mutex};
                        auto iter = employees.find(id);
                        if (iter == std::end(employees))
                            throw std::invalid_argument{"Employee with id \"" + id + "\" not found."};
                        iter->second = employee;
                    } },
                std::pair{"delete", [&]
                    (std::string const &id)
                    {
                        std::lock_guard loxk{mutex};
                        auto iter = employees.find(id);
                        if (iter == std::end(employees))
                            throw std::invalid_argument{"Employee with id \"" + id + "\" not found."};
                        employees.erase(iter);
                    } }
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
