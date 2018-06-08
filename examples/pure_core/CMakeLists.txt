cmake_minimum_required(VERSION 3.0.2)

include(ExternalProject)

project(pure_core)
set(PROJECT ${PROJECT_NAME})
string(TOLOWER "${PROJECT}" PROJECT_LC)

set (STD_CXX "c++17")

set (THITD_PARTY_SOURCES_PATH ${CMAKE_SOURCE_DIR}/../../third_party_sources)

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)
set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -std=${STD_CXX}")
set (CMAKE_CXX_FLAGS_RELEASE "-O3 -g0 -DNDEBUG")
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

#---------------------------------------------------------

#---------------------Third party common -----------------

set (THITD_PARTY_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/third_party)
set (THIRD_PARTY_PROJECTS "")
set (LIBRARIES "")


#-------------------nanorpc-------------------------------

set (NANORPC_PROJECT "nanorpc-project")
list (APPEND THIRD_PARTY_PROJECTS ${NANORPC_PROJECT})
set (NANORPC_SOURCE_PATH ${CMAKE_CURRENT_SOURCE_DIR}/../..)
set (NANORPC_INSTALL_DIR ${THITD_PARTY_OUTPUT_PATH}/nanorpc)
set (NANORPC_INCLUDE_DIR ${NANORPC_INSTALL_DIR}/include)
set (NANORPC_LIBRARIES_DIR ${NANORPC_INSTALL_DIR}/lib)
include_directories (SYSTEM ${NANORPC_INCLUDE_DIR})
link_directories(${NANORPC_LIBRARIES_DIR})

ExternalProject_Add (${NANORPC_PROJECT}
        SOURCE_DIR ${NANORPC_SOURCE_PATH}
        UPDATE_COMMAND ""
        CMAKE_ARGS -DCMAKE_INSTALL_PREFIX:INTERNAL=${NANORPC_INSTALL_DIR} 
            -DCMAKE_CXX_FLAGS:INTERNAL=${CMAKE_CXX_FLAGS} 
            -DCMAKE_C_FLAGS:INTERNAL=${CMAKE_C_FLAGS} 
            -DNANORPC_PURE_CORE:INTERNAL=ON
            -DNANORPC_WITH_SSL:INTERNAL=OFF
        LOG_DOWNLOAD 1
        LOG_UPDATE 1
        LOG_CONFIGURE 1
        LOG_BUILD 1
        LOG_TEST 1
        LOG_INSTALL 1
    )

list (INSERT LIBRARIES 0 nanorpc)

set (COMMON_HEADERS
    ${COMMON_HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

set (HEADERS
    ${HEADERS}
)

set(SOURCES
    ${SOURCES}
)

set (LIBRARIES
    ${LIBRARIES}
)

include_directories (include)
include_directories (${COMMON_HEADERS})

add_executable(${PROJECT_LC} ${HEADERS} ${SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
add_dependencies ("${PROJECT_LC}" ${THIRD_PARTY_PROJECTS})

