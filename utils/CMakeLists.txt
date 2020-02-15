#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
file(GLOB HDRS "*.h")
file(GLOB SRCS "*.cpp")

set(TARGET_NAME hddldemoutils)

add_library(${TARGET_NAME} SHARED ${HDRS} ${SRCS})
target_include_directories(${TARGET_NAME} PUBLIC ${Boost_INCLUDE_DIR} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${TARGET_NAME} PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${TARGET_NAME} PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_compile_definitions(${TARGET_NAME} PUBLIC BOOST_SPIRIT_THREADSAFE)

