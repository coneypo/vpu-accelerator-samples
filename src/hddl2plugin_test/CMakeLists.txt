set(HDDL2_TEST_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/${TARGET_NAME} ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE HDDL2_TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)
file(GLOB_RECURSE HDDL2_TEST_SRC_c ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

find_package(HddlUnite REQUIRED)

find_package(InferenceEngine REQUIRED)

find_package(OpenCV REQUIRED)

find_package(Threads REQUIRED)

set(TARGET_NAME hddl2_plugin_test)

add_executable(${TARGET_NAME} "${HDDL2_TEST_SRC}" "${HDDL2_TEST_SRC_c}"
                ../FullPipe/hddl2plugin_helper.cpp ../FullPipe/tinyYolov2_post.cpp ../FullPipe/ImageNetLabels.cpp)
target_include_directories(${TARGET_NAME} PUBLIC "$<BUILD_INTERFACE:${HDDL2_TEST_INC_DIR}>")

target_include_directories(${TARGET_NAME} PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../../inc/FullPipe")
target_include_directories(${TARGET_NAME} PUBLIC "${HDDLUNITE_INCLUDE_DIR}")
target_include_directories(${TARGET_NAME} PUBLIC "${InferenceEngine_INCLUDE_DIRS}")
target_include_directories(${TARGET_NAME} PUBLIC "${OpenCV_INCLUDE_DIRS}")

target_include_directories(${TARGET_NAME} PUBLIC "$<BUILD_INTERFACE:/home/kmb/kezhen/install/include>")
target_include_directories(${TARGET_NAME} PUBLIC "$<BUILD_INTERFACE:/home/kmb/cong/repos/kmb-plugin/include/hddl2>")
target_include_directories(${TARGET_NAME} PUBLIC "$<BUILD_INTERFACE:/home/kmb/cong/repos/kmb-plugin/src/hddl2_plugin/include>")

target_link_libraries(${TARGET_NAME} "${HDDLUNITE_LIB}")
target_link_libraries(${TARGET_NAME} "${OpenCV_LIBRARIES}")
target_link_libraries(${TARGET_NAME} "${InferenceEngine_LIBRARIES}")
target_link_libraries(${TARGET_NAME} "/home/kmb/kezhen/install/lib/libXLink.so")
target_link_libraries(${TARGET_NAME} Threads::Threads dl)

message("InferenceEngine_LIB: ${InferenceEngine_LIBRARIES}")

