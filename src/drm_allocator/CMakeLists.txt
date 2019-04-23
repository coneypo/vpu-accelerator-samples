
set(DRM_TYPE "0" CACHE STRING "DRM bo based Allocator  intel, hantro")

if (${DRM_TYPE} STREQUAL "intel" OR ${DRM_TYPE} STREQUAL "hantro")
    set(DRMALLOC_STATIC_LIB mp_drmalloc_static CACHE STRING "drm allocator static lib targets" FORCE)
    # find DRM_INCLUDE_DIRS/DRM_LIBRARIES
    find_package(PkgConfig REQUIRED)
    pkg_check_modules (DRM REQUIRED  libdrm libdrm_${DRM_TYPE})
    string(TOUPPER drm_${DRM_TYPE} DRM_DEFINITION)
    add_definitions(-D${DRM_DEFINITION})
    set(HDRS "mp_gstdrmbomemory.h")
    set(SRCS "mp_gstdrmbomemory.c")

    add_library(${DRMALLOC_STATIC_LIB} STATIC ${HDRS} ${SRCS})

    target_include_directories(${DRMALLOC_STATIC_LIB} PRIVATE ${DRM_INCLUDE_DIRS})
    target_include_directories(${DRMALLOC_STATIC_LIB} PRIVATE ${GSTREAMER_INCLUDE_DIRS})
    target_include_directories(${DRMALLOC_STATIC_LIB} PRIVATE ${GLIB_INCLUDE_DIRS})

    target_link_libraries(${DRMALLOC_STATIC_LIB} PRIVATE ${DRM_LIBRARIES})
    target_link_libraries(${DRMALLOC_STATIC_LIB} PRIVATE ${GSTREAMER_ALLOCATORS_LIBRARIES})
    target_link_libraries(${DRMALLOC_STATIC_LIB} PRIVATE ${GLIB_LIBRARIES})
    target_link_libraries(${DRMALLOC_STATIC_LIB} PRIVATE ${GLIB_GOBJECT_LIBRARIES})
    target_link_libraries(${DRMALLOC_STATIC_LIB} PRIVATE ${GSTREAMER_LIBRARIES})

    target_include_directories(${DRMALLOC_STATIC_LIB} PUBLIC ${CMAKE_SOURCE_DIR}/src/drm_allocator)
else()
    set(DRMALLOC_STATIC_LIB "" CACHE STRING "drm allocator static lib targets" FORCE)
    message("DRM_TYPE is unsupported or not set, skip build DRMALLOC_STATIC_LIB")
endif()
