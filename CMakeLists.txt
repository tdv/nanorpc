cmake_minimum_required(VERSION 3.0.2)

include(ExternalProject)

project(nanorpc)
set(PROJECT ${PROJECT_NAME})
string(TOLOWER "${PROJECT}" PROJECT_LC)

set (STD_CXX "c++17")

set (CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set (CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

set (THITD_PARTY_SOURCES_PATH ${CMAKE_SOURCE_DIR}/third_party_sources)

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

set (BOOST_PATH "${THITD_PARTY_SOURCES_PATH}/boost")

#------------------------ Boost --------------------------

#set (BOOST_ROOT ${THITD_PARTY_OUTPUT_PATH}/boost)

find_package(Boost 1.67.0)

if (Boost_FOUND)
    set (BOOST_INCLUDE_DIR ${Boost_INCLUDE_DIRS})
    set (BOOST_LIBRARIES_DIR ${Boost_LIBRARY_DIRS})
    set (BOOST_LIBRARIES ${Boost_LIBRARIES})
    include_directories(${BOOST_INCLUDE_DIR})
    link_directories(${BOOST_LIBRARIES_DIR})
else()
    set (BOOST_LIBRARIES
        "iostreams"
        "date_time"
        "serialization"
        "program_options"
        "log"
        "filesystem"
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

set(HEADERS
    ${HEADERS}
    ${CMAKE_CURRENT_SOURCE_DIR}/inslude/
)

set(SOURCES
    ${SOURCES}

    ${CMAKE_CURRENT_SOURCE_DIR}/src/nanorpc/http/client.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/nanorpc/http/server.cpp
)

set(LIBRARIES
    ${LIBRARIES}
)

include_directories (include)
include_directories (${HEADERS})

add_library (${PROJECT_LC} STATIC ${SOURCES})
if (NOT Boost_FOUND)
    add_dependencies ("${PROJECT_LC}" ${THIRD_PARTY_PROJECTS})
else()
    target_link_libraries (${PROJECT_LC} ${LIBRARIES})
endif()

install (TARGETS ${PROJECT_LC} EXPORT ${PROJECT_LC}
             ARCHIVE DESTINATION lib
)

install(DIRECTORY include/${PROJECT_LC} DESTINATION include)
