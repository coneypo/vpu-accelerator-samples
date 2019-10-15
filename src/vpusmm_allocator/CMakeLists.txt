# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

option(USE_VPUSMM "Use VPUSMM(VPU Shared memory manager) based allocator" OFF)
if(USE_VPUSMM)
    set(DMABUF_ALLOC_STATIC_LIB mp_vpusmmalloc_static CACHE STRING "VPUSMM allocator static lib targets" FORCE)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules (VPUSMM REQUIRED libvpusmm)
    set(HDRS "gstvpusmm.h")
    set(SRCS "gstvpusmm.c")
    add_library(${DMABUF_ALLOC_STATIC_LIB} STATIC ${HDRS} ${SRCS})

    target_include_directories(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${VPUSMM_INCLUDE_DIRS})
    target_include_directories(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    target_include_directories(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${GLIB_INCLUDE_DIRS})

    target_link_libraries(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${VPUSMM_LIBRARIES})
    target_link_libraries(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${GSTREAMER_ALLOCATORS_LIBRARIES})
    target_link_libraries(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${GLIB_LIBRARIES})
    target_link_libraries(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${GLIB_GOBJECT_LIBRARIES})
    target_link_libraries(${DMABUF_ALLOC_STATIC_LIB} PRIVATE ${GSTREAMER_LIBRARIES})

    target_include_directories(${DMABUF_ALLOC_STATIC_LIB} PUBLIC ${CMAKE_SOURCE_DIR}/src/vpusmm_allocator)

else()
    set(DMABUF_ALLOC_STATIC_LIB "" CACHE STRING "dmabuf allocator static lib targets" FORCE)
    message("skip build vpusmm based DMABUF_ALLOC_STATIC_LIB")
endif()
