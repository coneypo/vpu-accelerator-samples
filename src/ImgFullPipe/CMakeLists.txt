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

set(IMG_FULL_PIPE_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/FullPipe ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/ImgFullPipe ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE IMG_FULL_PIPE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../FullPipe/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)
file(GLOB_RECURSE IMG_FULL_PIPE_SRC_c ${CMAKE_CURRENT_SOURCE_DIR}/*.c ${CMAKE_CURRENT_SOURCE_DIR}/../FullPipe/*.c)

list(REMOVE_ITEM IMG_FULL_PIPE_SRC "${CMAKE_CURRENT_SOURCE_DIR}/../FullPipe/main.cpp")
list(REMOVE_ITEM IMG_FULL_PIPE_SRC_c "${CMAKE_CURRENT_SOURCE_DIR}/../FullPipe/main.c")

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

add_executable(ImgFullPipe "${IMG_FULL_PIPE_SRC}" "${IMG_FULL_PIPE_SRC_c}")
target_include_directories(ImgFullPipe PUBLIC "$<BUILD_INTERFACE:${IMG_FULL_PIPE_INC_DIR}>")

target_include_directories(ImgFullPipe PUBLIC "${HDDLUNITE_INCLUDE_DIR}")
target_include_directories(ImgFullPipe PUBLIC "${InferenceEngine_INCLUDE_DIRS}")
target_include_directories(ImgFullPipe PUBLIC "${OpenCV_INCLUDE_DIRS}")
target_include_directories(ImgFullPipe PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../inc/FullPipe/hddl2plugin")
# target_include_directories(FullPipe PUBLIC "$<BUILD_INTERFACE:/home/kmb/cong/repos/kmb-plugin/include/hddl2>")
# target_include_directories(FullPipe PUBLIC "$<BUILD_INTERFACE:/home/kmb/cong/repos/kmb-plugin/src/hddl2_plugin/include>")

target_link_libraries(ImgFullPipe "${HDDLUNITE_LIB}")
target_link_libraries(ImgFullPipe "${OpenCV_LIBRARIES}")
target_link_libraries(ImgFullPipe "${InferenceEngine_LIBRARIES}")
# target_link_libraries(FullPipe "/home/kmb/kezhen/install/lib/libXLink.so")
target_link_libraries(ImgFullPipe "/usr/lib/libXLink.so")

# message("InferenceEngine_LIB: ${InferenceEngine_LIBRARIES}")

target_link_libraries(ImgFullPipe Threads::Threads dl)

target_link_libraries(ImgFullPipe hva)

target_link_libraries(ImgFullPipe "${LIBVA_LIBRARIES}")

target_include_directories(ImgFullPipe PUBLIC "${LIBVA_INCLUDE_DIRS}")

target_link_libraries(ImgFullPipe "${LIBVA_DRM_LIBRARIES}")

target_include_directories(ImgFullPipe PUBLIC "${LIBVA_DRM_INCLUDE_DIRS}")

target_link_libraries(ImgFullPipe -L/opt/intel/gst/lib glib-2.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 gstapp-1.0 gstallocators-1.0 boost_filesystem)
target_include_directories(ImgFullPipe PUBLIC "${GSTREAMER_INCLUDE_DIRS}" "${GLIB_INCLUDE_DIRS}" "${BOOST_INC_DIR}")

target_include_directories(ImgFullPipe PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries(ImgFullPipe ${Boost_LIBRARIES})

target_include_directories(ImgFullPipe PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/vas/include")
target_link_libraries(ImgFullPipe "${CMAKE_CURRENT_SOURCE_DIR}/../../3rdparty/vas/lib/intel64/libvasot.so")
