project(hddldemo)

cmake_minimum_required(VERSION 3.13)
include(FindPkgConfig)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Debug)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/output)


include(ExternalProject)
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
add_subdirectory(tests)

install(FILES "${MSDK_GST_BINARY}/lib/release/libgstmfx.so" DESTINATION lib)