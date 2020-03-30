#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
file(GLOB HDRS "*.h")
file(GLOB SRCS "*.cpp")

set(TARGET_NAME hddldemoutils)
add_library(${TARGET_NAME} SHARED ${HDRS} ${SRCS})
target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wextra -Werror -fPIC -shared -fstack-protector-strong -Wformat -Wformat-security -Werror=format-security -fno-strict-overflow  -fno-delete-null-pointer-checks -fwrapv -D_FORTIFY_SOURCE=2)
target_include_directories(${TARGET_NAME} PUBLIC ${Boost_INCLUDE_DIR} ${PROJECT_SOURCE_DIR} ${CMAKE_CURRENT_LIST_DIR})
target_link_libraries(${TARGET_NAME} PRIVATE ${Boost_LIBRARIES})
target_link_libraries(${TARGET_NAME} PUBLIC ${CMAKE_THREAD_LIBS_INIT})
target_compile_definitions(${TARGET_NAME} PUBLIC BOOST_SPIRIT_THREADSAFE)

