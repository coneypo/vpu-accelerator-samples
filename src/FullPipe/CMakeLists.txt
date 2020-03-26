add_definitions(-DHAVE_VA_DRM)

find_package(LibVA REQUIRED)

find_package(Gstreamer REQUIRED)
find_package(Glib REQUIRED)

find_path(BOOST_INC_DIR NAMES boost/property_tree/json_parser.hpp
	HINTS $ENV{INSTALL_DIR}/include)
if(NOT BOOST_INC_DIR)
	message(FATAL_ERROR "BOOST header not found. hints path: $ENV{INSTALL_DIR}/include/boost")
endif()

find_package(Boost 1.53 COMPONENTS filesystem REQUIRED)

message("boost header: ${BOOST_INC_DIR}")

set(FULL_PIPE_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/FullPipe ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE FULL_PIPE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)
file(GLOB_RECURSE FULL_PIPE_SRC_c ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

# find_library(LibVA NAMES va HINTS "/usr/local/lib")
# if(NOT LibVA)
#     message(ERROR "LIBVA NOT FOUND!")
# endif()
# message(INFO "LibVA: ${LibVA}")

# find_library(LibVA-DRM NAMES va-drm HINTS "/usr/local/lib")
# if(NOT LibVA-DRM)
#     message(ERROR "LIBVA-DRM NOT FOUND!")
# endif()
# message(INFO "LibVA-DRM: ${LibVA-DRM}")

# find_path(LIBVA_INC_PATH NAMES va/va.h HINTS "/usr/local/include")
# if(NOT LIBVA_INC_PATH)
#     message(ERROR "libva header not found!")
# endif()
# message(INFO "LIBVA_INC_PATH: ${LIBVA_INC_PATH}")

find_package(Threads REQUIRED)

find_package(HddlUnite REQUIRED)

find_package(InferenceEngine REQUIRED)

find_package(OpenCV REQUIRED)

add_executable(FullPipe "${FULL_PIPE_SRC}" "${FULL_PIPE_SRC_c}")
target_include_directories(FullPipe PUBLIC "$<BUILD_INTERFACE:${FULL_PIPE_INC_DIR}>")

target_include_directories(FullPipe PUBLIC "${HDDLUNITE_INCLUDE_DIR}")
target_include_directories(FullPipe PUBLIC "${InferenceEngine_INCLUDE_DIRS}")
target_include_directories(FullPipe PUBLIC "${OpenCV_INCLUDE_DIRS}")
target_include_directories(FullPipe PUBLIC "$<BUILD_INTERFACE:/home/kmb/cong/repos/kmb-plugin/include/hddl2>")
target_include_directories(FullPipe PUBLIC "$<BUILD_INTERFACE:/home/kmb/cong/repos/kmb-plugin/src/hddl2_plugin/include>")

target_link_libraries(FullPipe "${HDDLUNITE_LIB}")
target_link_libraries(FullPipe "${OpenCV_LIBRARIES}")
target_link_libraries(FullPipe "${InferenceEngine_LIBRARIES}")
target_link_libraries(FullPipe "/home/kmb/kezhen/install/lib/libXLink.so")

# message("InferenceEngine_LIB: ${InferenceEngine_LIBRARIES}")

target_link_libraries(FullPipe Threads::Threads dl)

target_link_libraries(FullPipe hva)

target_link_libraries(FullPipe "${LIBVA_LIBRARIES}")

target_include_directories(FullPipe PUBLIC "${LIBVA_INCLUDE_DIRS}")

target_link_libraries(FullPipe "${LIBVA_DRM_LIBRARIES}")

target_include_directories(FullPipe PUBLIC "${LIBVA_DRM_INCLUDE_DIRS}")

target_link_libraries(FullPipe glib-2.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 gstapp-1.0 gstallocators-1.0 boost_filesystem)
target_include_directories(FullPipe PUBLIC "${GSTREAMER_INCLUDE_DIRS}" "${GLIB_INCLUDE_DIRS}" "${BOOST_INC_DIR}")

target_include_directories(FullPipe PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries(FullPipe ${Boost_LIBRARIES})

