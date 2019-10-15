# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

find_package(Glib REQUIRED)
find_package(Cairo REQUIRED)
find_package(Pango REQUIRED)
find_package(Json-c REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Gstreamer REQUIRED)
find_package(Boost REQUIRED COMPONENTS system filesystem)

add_subdirectory(utils)
add_subdirectory(config)
add_subdirectory(core)
add_subdirectory(modules)
add_subdirectory(drm_allocator)
add_subdirectory(vpusmm_allocator)