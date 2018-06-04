cmake_minimum_required(VERSION 3.0.2)

include(ExternalProject)

project(complex_type)
set(PROJECT ${PROJECT_NAME})
string(TOLOWER "${PROJECT}" PROJECT_LC)

set (PROJECT_SERVER_NAME ${PROJECT_LC}_server)
set (PROJECT_CLIENT_NAME ${PROJECT_LC}_client)

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

#------------------------ Boost --------------------------

find_package(Boost 1.67.0)
if (Boost_FOUND)

    set (BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
    set (BOOST_LIBRARIES_DIR ${Boost_LIBRARY_DIRS})
    set (BOOST_LIBRARIES ${Boost_LIBRARIES})
    include_directories(${BOOST_INCLUDE_DIR})
    link_directories(${BOOST_LIBRARIES_DIR})

else()

    set (BOOST_PATH "${THITD_PARTY_SOURCES_PATH}/boost")
    set (BOOST_ROOT ${THITD_PARTY_OUTPUT_PATH}/boost)

    set (BOOST_LIBRARIES
        "iostreams"
        "date_time"
        "thread"
        "system"
    )

    set (BOOST_INSTALL_DIR ${THITD_PARTY_OUTPUT_PATH}/boost)
    set (BOOST_INCLUDE_DIR ${BOOST_INSTALL_DIR}/include)
    set (BOOST_LIBRARIES_DIR ${BOOST_INSTALL_DIR}/lib)
    include_directories (SYSTEM ${BOOST_INCLUDE_DIR})
    link_directories(${BOOST_LIBRARIES_DIR})

    set (BOOST_PROJECT "boost-project")
    list (APPEND THIRD_PARTY_PROJECTS ${BOOST_PROJECT})

    foreach (lib ${BOOST_LIBRARIES})
        if (DEFINED BOOST_LIBRARIES_LIST)
            set (BOOST_LIBRARIES_LIST "${BOOST_LIBRARIES_LIST},")
        endif()
        set (BOOST_LIBRARIES_LIST "${BOOST_LIBRARIES_LIST}${lib}")
    endforeach()

    ExternalProject_Add(${BOOST_PROJECT}
            SOURCE_DIR ${BOOST_PATH}
            UPDATE_COMMAND ""
            BUILD_IN_SOURCE 1
            CONFIGURE_COMMAND ./bootstrap.sh --prefix=${BOOST_INSTALL_DIR} --with-libraries=${BOOST_LIBRARIES_LIST} --without-icu --without-icu
            BUILD_COMMAND ./b2 install -j8 --disable-icu --ignore-site-config "cxxflags=-std=${STD_CXX} -fPIC" link=static threading=multi runtime-link=static
            INSTALL_COMMAND ""
            LOG_DOWNLOAD 1
            LOG_UPDATE 1
            LOG_CONFIGURE 1
            LOG_BUILD 1
            LOG_TEST 1
            LOG_INSTALL 1
        )

    foreach (lib ${BOOST_LIBRARIES})
        list (APPEND LIBRARIES boost_${lib})
    endforeach()

endif()
#---------------------------------------------------------


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
            -DBOOST_ROOT:INTERNAL=${BOOST_ROOT}
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

set (SERVER_HEADERS
    ${SERVER_HEADERS}
)

set(SERVER_SOURCES
    ${SERVER_SOURCES}
)

set (CLIENT_HEADERS
    ${CLIENT_HEADERS}
)

set(CLIENT_SOURCES
    ${CLIENT_SOURCES}
)

set (LIBRARIES
    ${LIBRARIES}
    nanorpc
    boost_system
    pthread
    rt
)

include_directories (include)
include_directories (${COMMON_HEADERS})

add_executable(${PROJECT_SERVER_NAME} ${SERVER_HEADERS} ${SERVER_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp)
add_dependencies ("${PROJECT_SERVER_NAME}" ${THIRD_PARTY_PROJECTS})
target_link_libraries(${PROJECT_SERVER_NAME} ${LIBRARIES})

add_executable(${PROJECT_CLIENT_NAME} ${CLIENT_HEADERS} ${CLIENT_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/client/main.cpp)
add_dependencies ("${PROJECT_CLIENT_NAME}" ${THIRD_PARTY_PROJECTS})
target_link_libraries(${PROJECT_CLIENT_NAME} ${LIBRARIES})
