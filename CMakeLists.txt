project(hddldemo)
cmake_minimum_required(VERSION 3.13)

include(FindPkgConfig)
include(ExternalProject)

#required dependencies
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)
find_package(Boost REQUIRED system filesystem)
find_package(OpenCV 4.0.0 EXACT REQUIRED)

pkg_check_modules(GLIB2      REQUIRED  glib-2.0)
pkg_check_modules(GSTREAMER  REQUIRED  gstreamer-1.0)
pkg_check_modules(GSTVIDEO  REQUIRED  gstreamer-video-1.0)
pkg_check_modules(GSTAPP  REQUIRED  gstreamer-app-1.0)


set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -std=c++11")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS} -D_FORTIFY_SOURCE=2 -O2")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS} -g -ggdb -O0")

set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -z noexecstack -z relro -z now")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -Wl,--no-whole-archive")

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)



ExternalProject_Add(msdk-gst 
    	GIT_REPOSITORY https://github.com/intel/gstreamer-media-SDK.git
	GIT_TAG "a4fce96f4a0acb98815875e39022d43a1541b663"
	INSTALL_DIR ${CMAKE_BINARY_DIR}/output
	CMAKE_ARGS 
		-DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/output

)

ExternalProject_Get_property(msdk-gst SOURCE_DIR)
ExternalProject_Get_property(msdk-gst BINARY_DIR)

set(MSDK_GST_SOURCE ${SOURCE_DIR})
set(MSDK_GST_BINARY ${BINARY_DIR})

add_subdirectory(pipeline)
add_subdirectory(app)
add_subdirectory(gstreamer_plugin)
add_subdirectory(utils)
add_subdirectory(common)
add_subdirectory(tests)

install(FILES "${MSDK_GST_BINARY}/lib/release/libgstmfx.so" DESTINATION lib)
