#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
file(GLOB SRCS "*.h")
aux_source_directory(. SRCS)
add_library(utils_osl STATIC ${SRCS})
target_include_directories(utils_osl PUBLIC ${CMAKE_SOURCE_DIR})
target_compile_options(utils_osl PUBLIC ${HddlCompileOptions})