//-------------------------------------------------------------------
//  Nano RPC
//  https://github.com/tdv/nanorpc
//  Created:     05.2018
//  Copyright (C) 2018 tdv
//-------------------------------------------------------------------

#ifndef __EXAMPLES_COMPLEX_TYPE_DATA_H__
#define __EXAMPLES_COMPLEX_TYPE_DATA_H__

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

#endif  // !__EXAMPLES_COMPLEX_TYPE_DATA_H__
