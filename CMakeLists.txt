cmake_minimum_required(VERSION 2.8)
project(mediapipe2)

#set this property for subdir install command
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(${CMAKE_SOURCE_DIR}/cmake/mediapipe.cmake)

set(BUILD_APP "all" CACHE STRING "Choose application to build")
set_property(CACHE BUILD_APP PROPERTY STRINGS ipc hddl manager all)
if(${BUILD_APP} STREQUAL ipc)
    message(STATUS "will build ipc")
    add_subdirectory(front/ipc_mediapipe)
elseif(${BUILD_APP} STREQUAL hddl)
    message(STATUS "will build hddl")
    add_subdirectory(front/hddl_mediapipe)
elseif(${BUILD_APP} STREQUAL manager)
    message(STATUS "will build manager")
    add_subdirectory(front/hddl_mediapipe3)
    add_subdirectory(front/hddl_manager)
elseif(${BUILD_APP} STREQUAL all)
    message(STATUS "will build all app")
    add_subdirectory(front/ipc_mediapipe)
    add_subdirectory(front/hddl_mediapipe)
else()
    message(FATAL_ERROR "BUILD_APP set to unknown option")
endif()

