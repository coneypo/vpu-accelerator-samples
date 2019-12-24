#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
include(${PROJECT_SOURCE_DIR}/cmake/utils.cmake)

file(GLOB HDRS "*.h")
file(GLOB SRCS "*.cpp")

add_subdirectory(platform)

set(TARGET_NAME utils_static)
set(BOOST_HASHER_HEADER "boost/compute/detail/sha1.hpp")
set(BOOST_HASHER_CLASS "boost::uuids::detail::sha1")

configure_file(SecurityUtils.cpp.in ${CMAKE_CURRENT_BINARY_DIR}/SecurityUtils.cpp)

add_library(${TARGET_NAME} STATIC ${HDRS} ${SRCS} ${CMAKE_CURRENT_BINARY_DIR}/SecurityUtils.cpp)
target_include_directories(${TARGET_NAME} PUBLIC ${Boost_INCLUDE_DIR} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${TARGET_NAME} PRIVATE utils_osl ${Boost_LIBRARIES})
target_compile_definitions(${TARGET_NAME} PUBLIC BOOST_SPIRIT_THREADSAFE)
target_compile_options(${TARGET_NAME} PUBLIC ${HddlCompileOptions})
set_target_output_dirs(${TARGET_NAME} lib lib bin)

if(COLOR_LOG)
    message(STATUS "Enabled COLOR_LOG")
    target_compile_definitions(${TARGET_NAME} PUBLIC COLOR_LOG)
else()
    message(STATUS "Disabled COLOR_LOG")
endif()

if(VERBOSE_LOG)
    message(STATUS "Enabled VERBOSE_LOG")
    target_compile_definitions(${TARGET_NAME} PUBLIC VERBOSE_LOG)
else()
    message(STATUS "Disabled VERBOSE_LOG")
endif()
