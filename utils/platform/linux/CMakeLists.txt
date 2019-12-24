#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
file(GLOB SRCS "*.h")
aux_source_directory(. SRCS)
add_library(utils_osl OBJECT ${SRCS})
target_include_directories(utils_osl
    PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}
    PRIVATE ${PROJECT_SOURCE_DIR})
target_link_libraries(utils_osl PRIVATE pthread rt)
target_compile_options(utils_osl PUBLIC ${HddlCompileOptions})