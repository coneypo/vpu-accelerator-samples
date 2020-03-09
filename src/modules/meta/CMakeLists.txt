# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

set(TARGET_NAME mp_meta_static)

file(GLOB HDRS "*.h")
aux_source_directory(. SRCS)

add_library(${TARGET_NAME} STATIC ${HDRS} ${SRCS})

target_include_directories(
    ${TARGET_NAME} PUBLIC
    ${LIBHARFBUZZ_INCLUDE_DIRS}
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${PROJECT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ${TARGET_NAME} PRIVATE
    ${CAIRO_LIBRARIES}
)

