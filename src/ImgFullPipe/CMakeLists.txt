find_path(BOOST_INC_DIR NAMES boost/property_tree/json_parser.hpp
	HINTS $ENV{INSTALL_DIR}/include)
if(NOT BOOST_INC_DIR)
	message(FATAL_ERROR "BOOST header not found. hints path: $ENV{INSTALL_DIR}/include/boost")
endif()

find_package(Boost 1.53 COMPONENTS filesystem REQUIRED)

message("boost header: ${BOOST_INC_DIR}")

set(IMG_FULL_PIPE_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/ImgFullPipe 
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util 
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/inference_util
                            ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/nodes)

file(GLOB_RECURSE IMG_FULL_PIPE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp 
                                    ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp 
                                    ${CMAKE_CURRENT_SOURCE_DIR}/../inference_util/*.cpp
                                    ${CMAKE_CURRENT_SOURCE_DIR}/../nodes/*.cpp)
file(GLOB_RECURSE IMG_FULL_PIPE_SRC_c ${CMAKE_CURRENT_SOURCE_DIR}/*.c)

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

target_link_libraries(ImgFullPipe "${HDDLUNITE_LIB}")
target_link_libraries(ImgFullPipe "${OpenCV_LIBRARIES}")
target_link_libraries(ImgFullPipe "${InferenceEngine_LIBRARIES}")

target_link_libraries(ImgFullPipe "/usr/lib/libXLink.so")

target_link_libraries(ImgFullPipe Threads::Threads dl)

target_link_libraries(ImgFullPipe hva)

target_link_libraries(ImgFullPipe boost_filesystem)
target_include_directories(ImgFullPipe PUBLIC "${BOOST_INC_DIR}")

target_include_directories(ImgFullPipe PUBLIC ${Boost_INCLUDE_DIR})
target_link_libraries(ImgFullPipe ${Boost_LIBRARIES})