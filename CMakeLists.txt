PROJECT(mediapipe2)
cmake_minimum_required(VERSION 2.8)

#set this property for subdir install command
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

if(BUILD_APP STREQUAL ipc)
    message("will build ipc")
    add_subdirectory(front/ipc_mediapipe)
elseif(BUILD_APP STREQUAL hddl)
    message("will build hddl")
elseif(BUILD_APP STREQUAL manage)
    message("will build manage")
elseif(BUILD_APP STREQUAL all)
    message("will build all app")
    add_subdirectory(front/ipc_mediapipe)
else(BUILD_APP STREQUAL ipc)
    message("will use default, build all app")
    add_subdirectory(front/ipc_mediapipe)
endif(BUILD_APP STREQUAL ipc)

