//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

// STD
#include <cstdlib>
#include <iostream>

// NANORPC
#include <nanorpc/http/easy.h>

// THIS
#include "common/data.h"

int main()
{
    try
    {
        auto client = nanorpc::http::easy::make_client("localhost", "55555", 8, "/api/");

        std::string employee_id = "employee_1";

        {
            data::employee employee;

            employee.name = "John";
            employee.last_name = "Brown";
            employee.age = 33;
            employee.company = "Google";    // Johns dreams
            employee.occupation = data::occupation_type::developer;
            employee.job.push_back({"Task 1", "Do something."});
            employee.job.push_back({"Task 2", "Do something more."});

            employee_id = client.call("create", employee_id, employee).as<std::string>();
            std::cout << "added employee with id \"" << employee_id << "\"." << std::endl;
        }

        auto show_employee_info = [] (data::employee const &employee)
            {
                std::cout << "name: " << employee.name << std::endl;
                std::cout << "last_name: " << employee.last_name << std::endl;
                std::cout << "age: " << employee.age << std::endl;
                std::cout << "company: " << employee.company << std::endl;
                std::cout << "occupation: "
                          << (employee.occupation == data::occupation_type::developer ? "developer" : "manager")
                          << std::endl;
                for (auto const &task : employee.job)
                {
                    std::cout << "\ttask name: " << task.name << std::endl;
                    std::cout << "\ttask description: " << task.description << std::endl;
                }
            };

        data::employee employee = client.call("read", employee_id);

        std::cout << "about employee with id \"" << employee_id << "\"" << std::endl;
        show_employee_info(employee);

        employee.occupation = data::occupation_type::manager;

        client.call("update", employee_id, employee);
        std::cout << "the employee has been promoted ..." << std::endl;

        employee = client.call("read", employee_id).as<data::employee>();

        std::cout << "new info about employee with id \"" << employee_id << "\"" << std::endl;
        show_employee_info(employee);

        client.call("delete", employee_id);
        std::cout << "the employee has been fired ..." << std::endl;

        std::cout << "you can't fire an employee twice" << std::endl;
        client.call("delete", employee_id);
    }
    catch (std::exception const &e)
    {
        std::cerr << "Error: " << nanorpc::core::exception::to_string(e) << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
