# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

cmake_minimum_required(VERSION 2.8)
project(mediapipe2)

#set this property for subdir install command
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

list(APPEND CMAKE_MODULE_PATH
    ${PROJECT_SOURCE_DIR}/src/modules/cmake/modules
    ${PROJECT_SOURCE_DIR}/cmake/modules)

include(${CMAKE_SOURCE_DIR}/cmake/mediapipe.cmake)

set(BUILD_APP "all" CACHE STRING "Choose application to build")
set_property(CACHE BUILD_APP PROPERTY STRINGS ipc hddl manager all)

option(MULTI_THREAD_MODE "Create mediapipe in standalone thread" OFF)
option(USE_THREAD_DEFAULT_MAIN_CONTEXT "Use thread default main context" OFF)
if(MULTI_THREAD_MODE)
    add_definitions(-DMULTI_THREAD_MODE)
endif()

if(${BUILD_APP} STREQUAL ipc)
    message(STATUS "will build ipc")
    add_subdirectory(front/ipc_mediapipe)
elseif(${BUILD_APP} STREQUAL hddl)
    message(STATUS "will build hddl")
    add_subdirectory(src)
    add_subdirectory(front/hddl_mediapipe)
elseif(${BUILD_APP} STREQUAL manager)
    message(STATUS "will build manager")
    add_subdirectory(src)
    if (NOT MULTI_THREAD_MODE)
        add_subdirectory(front/hddl_mediapipe3)
    endif ()
    add_subdirectory(front/hddl_manager)
elseif(${BUILD_APP} STREQUAL all)
    message(STATUS "will build all app")
    add_subdirectory(front/ipc_mediapipe)
    add_subdirectory(src)
    add_subdirectory(front/hddl_mediapipe)
else()
    message(FATAL_ERROR "BUILD_APP set to unknown option")
endif()

