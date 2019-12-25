pkg_check_modules(GLIB2      REQUIRED  glib-2.0)
pkg_check_modules(GSTREAMER  REQUIRED  gstreamer-1.0)
pkg_check_modules(OpenCV     REQUIRED  opencv4)
pkg_check_modules(GSTVIDEO   REQUIRED  gstreamer-video-1.0)

include_directories(${GLIB2_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${OpenCV_INCLUDE_DIRS})

set(PLUGIN_ROOT_DIR ${CMAKE_SOURCE_DIR}/gstreamer_plugin)

include_directories(
    /opt/intel/mediasdk/include/
    ${MSDK_GST_SOURCE}
    ${MSDK_GST_SOURCE}/gst/mfx
    ${MSDK_GST_SOURCE}/gst-libs/mfx
    ${MSDK_GST_BINARY}
    )



if(UNIX)
    include_directories(${LIBVA_INCLUDE_DIRS})
endif()


set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -fstack-protector-strong -shared -fPIE -fPIC -O2 -D_FORTIFY_SOURCE=2 -Wformat -Wformat-security -Wall")
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wl,-no-as-needed")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS} -D_FORTIFY_SOURCE=2 -O2")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS} -g -ggdb -O0")

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2 -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -ggdb -O0")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z noexecstack -z relro -z now")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-whole-archive")

link_directories(/usr/lib)

add_subdirectory(gst-lib)
add_subdirectory(gst)
