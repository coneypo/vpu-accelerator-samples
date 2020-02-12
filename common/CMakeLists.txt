#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
set(CMAKE_AUTOMOC ON)
file(GLOB HDRS "*.h")
file(GLOB SRCS "*.cpp")

set(TARGET_NAME common_static)

add_library(${TARGET_NAME} STATIC ${HDRS} ${SRCS})

target_include_directories(${TARGET_NAME} PUBLIC
        ${PROJECT_SOURCE_DIR}
       	${CMAKE_CURRENT_LIST_DIR}
       	${Boost_INCLUDE_DIR}
        ${Qt5Widgets_INCLUDE_DIR}
        ${Qt5Network_INCLUDE_DIR})
target_link_libraries(${TARGET_NAME} PUBLIC
        ${Qt5Widgets_LIBRARIES}
        ${Qt5Network_LIBRARIES}
       	${Boost_LIBRARIES})

target_compile_definitions(${TARGET_NAME} PUBLIC BOOST_SPIRIT_THREADSAFE)
set_target_output_dirs(${TARGET_NAME} lib lib bin)
