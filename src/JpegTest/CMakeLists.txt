set(JPEG_TEST_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../../inc")

file(GLOB_RECURSE JPEG_TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(JpegTest "${SAMPLE_SRC}")
target_include_directories(JpegTest PUBLIC "$<BUILD_INTERFACE:${SAMPLE_INC_DIR}>")

target_link_libraries(JpegTest pthread)

target_link_libraries(JpegTest hva)
