# nanorpc - lightweight RPC in pure C++ 17
Nano RPC is a lightweight RPC in C++ 17 with support for user-defined data structures, without code generation and without macros, only pure C++ with HTTP transport. If you need SSL, you can use nginx as frontend. Nano RPC is a very light library.  

# Version
1.0.0  

# Features
- base for client-server applications
- simple reflection for users structures in pure C++
- support for nested structures
- NO macros
- NO code generation
- you can use types from stl, such as vector, list, set, map, string, etc. and similar types from the boost library
- customization for serialization and transtorpt and easy interface for beginners
- HTTP transport based on boost.asio and boost.beast  

**NOTE**  
Currently, C++ reflection is not supported out of the box, so this library has some restrictions in using types for function parameters and for return values.  

## Restrictions
- user-defined data structures should not have a user-defined constructor
- no inheritance
- you can not use all types from stl and boost
- you can't use raw pointers and non-const references  

# Compiler
The minimum compiler version required is gcc 7.3 (other compilers were not tested)  

# OS
Linux (Tested on Ubuntu 16.04)  

**NOTE**  
The code is a cross-platform. Perhaps you will be able to compile under another OS with another compiler, with your own modifications for a build script.  

# Dependencies
- Boost only  

# Build and install
```bash
git clone https://github.com/tdv/nanorpc.git  
cd nanorpc  
./download_third_party.sh  
mkdir build  
cd build  
cmake ..  
make  
make install  
```

You can try using CMAKE_INSTALL_PREFIX to select the installation directory  

**NOTE**  
If you have installed boost 1.67 or later  in your system, you can build library without running download_third_party.sh  

## Build examples
```bash
cd nanorpc/examples/{sample_project}
mkdir build  
cd build  
cmake ..  
make  
```

# Examples

## RPC. Hello World
[Source code](https://github.com/tdv/nanorpc/tree/master/examples/hello_world)  
**Description**  
The "Hello World" example demonstrates a basic client-server application with RPC and HTTP communication.  

**Server application**  
```cpp
// STD
#include <cstdlib>
#include <iostream>

// NANORPC
#include <nanorpc/http/easy.h>

int main()
{
    try
    {
        auto server = nanorpc::http::easy::make_server("0.0.0.0", "55555", 8, "/api/",
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
```
**Client application**  
```cpp
// STD
#include <cstdlib>
#include <iostream>

// NANORPC
#include <nanorpc/http/easy.h>

int main()
{
    try
    {
        auto client = nanorpc::http::easy::make_client("localhost", "55555", 8, "/api/");

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

```

## Complex Type
[Source code](https://github.com/tdv/nanorpc/tree/master/examples/complex_type)  
**Description**  
This example is the same as "Hello World". The difference is in calling remote methods with user-defined data structures as parameters and returning a value. The project structure is the same as in the previous project example, we only add the definition of user-defined data structures.  

**Common data**  
```cpp
// STD
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace data
{

enum class occupation_type
{
    unknown,
    developer,
    manager
};

struct task
{
    std::string name;
    std::string description;
};

using tasks = std::vector<task>;

struct employee
{
    std::string name;
    std::string last_name;
    std::uint16_t age;
    std::string company;
    occupation_type occupation;
    tasks job;
};

using employees = std::map<std::string, employee>;

}
```

**Server application**  
```cpp
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

```

**Client application**  
```cpp
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

```
