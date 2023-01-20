set(PROJECT ${PROJECT_NAME})
string(TOLOWER "${PROJECT}" PROJECT_LC)

set (PROJECT_SERVER_NAME ${PROJECT_LC}_server)
set (PROJECT_CLIENT_NAME ${PROJECT_LC}_client)

set (STD_CXX "c++17")

set (CMAKE_MODULE_PATH ${PROJECT_SOURCE_DIR}/MyCMakeScripts)
set (EXECUTABLE_OUTPUT_PATH ${CMAKE_SOURCE_DIR}/bin)

set (CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -W -Wall -std=${STD_CXX}")
set (CMAKE_CXX_FLAGS_RELEASE "-O3 -g0 -DNDEBUG")
set (CMAKE_POSITION_INDEPENDENT_CODE ON)

#---------------------------------------------------------

#---------------------- Dependencies ---------------------

if (NOT DEFINED BOOST_ROOT)
    find_package(Boost 1.67.0 REQUIRED)
    if (NOT DEFINED Boost_FOUND)
        message(FATAL_ERROR "Boost_INCLUDE_DIRS is not found.")
    endif()
else()
    set(Boost_INCLUDE_DIRS "${BOOST_ROOT}include")
    set(Boost_LIBRARY_DIRS "${BOOST_ROOT}lib")
endif()

include_directories(${Boost_INCLUDE_DIRS})
link_directories(${Boost_LIBRARY_DIRS})

add_definitions(-DBOOST_ERROR_CODE_HEADER_ONLY)

set (Boost_LIBRARIES
    ${Boost_LIBRARIES}
    boost_iostreams
    boost_date_time
    boost_thread
    boost_system
)


find_package(nanorpc REQUIRED)
include_directories(${NANORPC_INCLUDE_DIR})
link_directories(${NANORPC_LIBS_DIR})

#---------------------------------------------------------

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
    ${NANORPC_LIBRARIES}
    ${Boost_LIBRARIES}
    ssl
    crypto
    pthread
    rt
)

include_directories (include)
include_directories (${COMMON_HEADERS})

add_executable(${PROJECT_SERVER_NAME} ${SERVER_HEADERS} ${SERVER_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/server/main.cpp)
target_link_libraries(${PROJECT_SERVER_NAME} ${LIBRARIES})

add_executable(${PROJECT_CLIENT_NAME} ${CLIENT_HEADERS} ${CLIENT_SOURCES} ${CMAKE_CURRENT_SOURCE_DIR}/client/main.cpp)
target_link_libraries(${PROJECT_CLIENT_NAME} ${LIBRARIES})
