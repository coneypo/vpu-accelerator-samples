add_definitions(-DHAVE_VA_DRM)

find_package(LibVA REQUIRED)

find_package(Gstreamer REQUIRED)
find_package(Glib REQUIRED)

find_path(BOOST_INC_DIR NAMES boost/property_tree/json_parser.hpp
	HINTS $ENV{INSTALL_DIR}/include)
if(NOT BOOST_INC_DIR)
	message(FATAL_ERROR "BOOST header not found. hints path: $ENV{INSTALL_DIR}/include/boost")
endif()

message("boost header: ${BOOST_INC_DIR}")

set(DEC_WITH_JPEG_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/DecWithJpeg ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE DEC_WITH_JPEG_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)
file(GLOB_RECURSE DEC_WITH_JPEG_SRC_c ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

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

add_executable(DecWithJpeg "${DEC_WITH_JPEG_SRC}" "${DEC_WITH_JPEG_SRC_c}")
target_include_directories(DecWithJpeg PUBLIC "$<BUILD_INTERFACE:${DEC_WITH_JPEG_INC_DIR}>")

target_include_directories(DecWithJpeg PUBLIC "${HDDLUNITE_INCLUDE_DIR}")
target_link_libraries(DecWithJpeg "${HDDLUNITE_LIB}")

target_link_libraries(DecWithJpeg Threads::Threads)

target_link_libraries(DecWithJpeg hva)

target_link_libraries(DecWithJpeg "${LIBVA_LIBRARIES}")

# target_include_directories(JpegTest PUBLIC ${LIBVA_INC_PATH})
target_include_directories(DecWithJpeg PUBLIC "${LIBVA_INCLUDE_DIRS}")

get_target_property(DEC_WITH_JPEG_INC_TEST DecWithJpeg INCLUDE_DIRECTORIES)
message("INCLUDE_DIRECTORIES: ${DEC_WITH_JPEG_INC_TEST}")
