# nanorpc - lightweight RPC in pure C++ 17
Nano RPC is a lightweight RPC in C++ 17 with support for user-defined data structures, 
without code generation and without macros, only pure C++ with HTTP/HTTPS transport.   

# Version
1.1.1  

# Features
- base for client-server applications
- simple reflection for users structures in pure C++
- support for nested structures
- NO macros
- NO code generation
- you can use types from STL, such as vector, list, set, map, string, etc. and similar types from the boost library
- customization for serialization and transport and easy interface for beginners
- the build in the pure mode for usage with your own transport (without boost)  
- HTTP/HTTPS transport based on boost.asio and boost.beast  

**NOTE**  
Currently, C++ reflection is not supported out of the box, 
so this library has some restrictions in using types for function parameters and return values.  

## Restrictions
- user-defined data structures should not have a user-defined constructor
- no inheritance
- you can't use arbitrary types from STL and boost
- you can't use raw pointers and non-const references  

# Compiler
The minimum compiler version required is gcc 7.3 (other compilers were not tested)  

# OS
Linux (Tested on Ubuntu 16.04 and Ubuntu 18.04)

**NOTE**  
The code is cross-platform. Perhaps you will be able to compile under another OS with another compiler, 
with your own modifications for a build script.  

# Dependencies
- Boost only  

# Build and install

## Clone and build with installed Boost  
```bash
git clone https://github.com/tdv/nanorpc.git  
cd nanorpc
mkdir build  
cd build  
cmake ..  
make  
make install  
```
You can try using CMAKE_INSTALL_PREFIX to select the installation directory  

## Clone and build without installed Boost  
```bash
git clone https://github.com/tdv/nanorpc.git  
cd nanorpc
./build_with_boost.sh
```

**NOTE**  
NanoRPC has two build options  
- with SSL  
- pure core only  

Use cmake -D with NANORPC_WITH_SSL or NANORPC_PURE_CORE. You can't enable both options at the same time.  
The 'pure core' build you can use with your own transport.  

## Build examples
### Build examples with installed boost and nanorpc
```bash
cd examples/{example_project}
mkdir build  
cd build  
cmake ..  
make  
```
### Build examples without installed boost and nanorpc
```bash
cd examples/{example_project}
mkdir build  
cd build  
cmake -DBOOST_ROOT=$PWD/../../../third_party/boost -Dnanorpc_DIR=$PWD/../../../target/nanorpc ..
make  
```

# Examples

## Hello World
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
This example is the same as "Hello World". 
The difference is in calling remote methods with user-defined data structures as parameters and returning a value. 
The project structure is the same as in the previous example, 
only the definitions of the user-defined data structures were added.  

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

## Pure Core
[Source code](https://github.com/tdv/nanorpc/tree/master/examples/pure_core)  
**Description**  
The "Pure Core" example demonstrates a basic client-server application with RPC and in-memory (in one process) communication. 
In this example 'executor' is a transport stub and you can rewrite it with your own transport implementation.  

**Application**  
```cpp
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

```

## SSL Hello World
[Source code](https://github.com/tdv/nanorpc/tree/master/examples/ssl_hello_world)  
**Description**  
The "SSL Hello World" example demonstrates a basic client-server application with RPC and HTTPS communication. 
The example is similar to 'Hello World' example with HTTPS transport. 
The example must be executed with certificate files. For test you can generate your own certificates  
```bash
cd examples/ssl_hello_world/bin
openssl dhparam -out dh.pem 2048
openssl req -newkey rsa:2048 -nodes -keyout key.pem -x509 -days 10000 -out cert.pem \
    -subj "//C=US\ST=CA\L=Los Angeles\O=Beast\CN=www.example.com"
```
In this example you should run the client and server applications from the folder with certificates.  

# Benchmark with ab utility
- create a file with the request data dump (request.txt)  
- run the hello_world_server sample
- run ab utility with request.txt  

**Create dump (request.txt)**  
```bash
echo '1 1 15118982290295364091 "test"  ' >request.txt
```

**Run ab utility**  
```bash
ab -c 1000 -n 1000000 -r -k -p request.txt "http://localhost:55555/api/"
```

**Results**  
```bash
Server Software:        NanoRPC
Server Hostname:        localhost
Server Port:            55555

Document Path:          /api/
Document Length:        21 bytes

Concurrency Level:      1000
Time taken for tests:   29.444 seconds
Complete requests:      1000000
Failed requests:        0
Keep-Alive requests:    1000000
Total transferred:      134000000 bytes
Total body sent:        192000000
HTML transferred:       21000000 bytes
Requests per second:    33962.98 [#/sec] (mean)
Time per request:       29.444 [ms] (mean)
Time per request:       0.029 [ms] (mean, across all concurrent requests)
Transfer rate:          4444.37 [Kbytes/sec] received
                        6368.06 kb/s sent
                        10812.43 kb/s total

Connection Times (ms)
              min  mean[+/-sd] median   max
Connect:        0    0   0.9      0      44
Processing:     0   29  15.0     28     227
Waiting:        0   29  15.0     28     227
Total:          0   29  15.0     28     240

Percentage of the requests served within a certain time (ms)
  50%     28
  66%     33
  75%     36
  80%     39
  90%     48
  95%     57
  98%     67
  99%     75
 100%    240 (longest request)
```


Take a look on the following line from the above results  
```bash
Requests per second:    33962.98 [#/sec] (mean)
```
I think this is a good result of using nanorpc with a simple HTTP server based on boost.  
