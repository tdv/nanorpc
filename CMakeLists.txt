cmake_minimum_required(VERSION 3.0.2)

include(ExternalProject)

#-----------------------Options--------------------------------------
option (NANORPC_WITH_SSL "[NANORPC] Support working with SSL" ON)
option (NANORPC_PURE_CORE "[NANORPC] Only pure core" OFF)
#--------------------------------------------------------------------

#-----------------------Version--------------------------------------
set (NANORPC_VERSION_MAJOR 1)
set (NANORPC_VERSION_MINOR 1)
set (NANORPC_VERSION_PATCH 0)
#--------------------------------------------------------------------

project(nanorpc)
set(PROJECT ${PROJECT_NAME})
string(TOLOWER "${PROJECT}" PROJECT_LC)

set (STD_CXX "c++17")

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -std=${STD_CXX}")
set (CMAKE_CXX_FLAGS_RELEASE "-O3 -g0 -DNDEBUG")
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

#---------------------------------------------------------

if (NANORPC_PURE_CORE AND NANORPC_WITH_SSL)
    message(FATAL_ERROR "You can't use the NANORPC_PURE_CORE and NANORPC_WITH_SSL options together.")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/nanorpc/version/library.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/nanorpc/version/library.h)

if (NANORPC_WITH_SSL)
    set (NANORPC_COMPILER_DEFINES "${NANORPC_COMPILER_DEFINES}\n#define NANORPC_WITH_SSL")
endif()

if (NANORPC_PURE_CORE)
    set (NANORPC_COMPILER_DEFINES "${NANORPC_COMPILER_DEFINES}\n#define NANORPC_PURE_CORE")
endif()

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/src/nanorpc/core/config.h.in ${CMAKE_CURRENT_SOURCE_DIR}/include/nanorpc/core/detail/config.h)

#---------------------------------------------------------

set (LIBRARIES "")

#------------------------ Boost --------------------------
if (NOT NANORPC_PURE_CORE)

    find_package(Boost 1.66.0)

    if (Boost_FOUND)
        set (LIBRARIES
            ${LIBRARIES}
            ${Boost_LIBRARIES}
        )
        include_directories(${Boost_INCLUDE_DIRS})
        link_directories(${Boost_LIBRARY_DIRS})
    else()
        message(FATAL_ERROR "Boost version 1.66 or later not found.")
    endif()

endif()
#---------------------------------------------------------

set(HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/inslude/
)

set(SOURCES
    ${SOURCES}
)

if (NOT NANORPC_PURE_CORE)
    set(SOURCES
        ${SOURCES}

        ${CMAKE_CURRENT_SOURCE_DIR}/src/nanorpc/http/client.cpp
        ${CMAKE_CURRENT_SOURCE_DIR}/src/nanorpc/http/server.cpp
    )
endif()

set(LIBRARIES
    ${LIBRARIES}
)

include_directories (include)
include_directories (${HEADERS})

if (NOT NANORPC_PURE_CORE)
    add_library (${PROJECT_LC} STATIC ${SOURCES})
    target_link_libraries (${PROJECT_LC} ${LIBRARIES})
endif()

if (NOT NANORPC_PURE_CORE)
    install (TARGETS ${PROJECT_LC} EXPORT ${PROJECT_LC}
                 ARCHIVE DESTINATION lib
    )
endif()

install(DIRECTORY include/${PROJECT_LC} DESTINATION include)
