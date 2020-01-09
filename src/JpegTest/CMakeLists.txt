add_definitions(-DHAVE_VA_DRM)

set(JPEG_TEST_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../inc")

file(GLOB_RECURSE JPEG_TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)
file(GLOB_RECURSE JPEG_TEST_SRC_c ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

find_library(LibVA NAMES va HINTS "/usr/local/lib")
if(NOT LibVA)
    message(ERROR "LIBVA NOT FOUND!")
endif()
message(INFO "LibVA: ${LibVA}")

find_library(LibVA-DRM NAMES va-drm HINTS "/usr/local/lib")
if(NOT LibVA-DRM)
    message(ERROR "LIBVA-DRM NOT FOUND!")
endif()
message(INFO "LibVA-DRM: ${LibVA-DRM}")

find_path(LIBVA_INC_PATH NAMES va/va.h HINTS "/usr/local/include")
if(NOT LIBVA_INC_PATH)
    message(ERROR "libva header not found!")
endif()
message(INFO "LIBVA_INC_PATH: ${LIBVA_INC_PATH}")

find_package(Threads REQUIRED)

add_executable(JpegTest "${JPEG_TEST_SRC}" "${JPEG_TEST_SRC_c}")
target_include_directories(JpegTest PUBLIC "$<BUILD_INTERFACE:${JPEG_TEST_INC_DIR}>")

target_link_libraries(JpegTest Threads::Threads)

target_link_libraries(JpegTest hva ${LibVA} ${LibVA-DRM})

target_include_directories(JpegTest PUBLIC ${LIBVA_INC_PATH})

get_target_property(JPEG_INC_TEST JpegTest INCLUDE_DIRECTORIES)
message(INFO "INCLUDE_DIRECTORIES: ${JPEG_INC_TEST}")
