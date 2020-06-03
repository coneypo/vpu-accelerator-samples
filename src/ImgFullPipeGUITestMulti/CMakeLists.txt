find_package(Boost 1.53 COMPONENTS filesystem REQUIRED)

find_package(Threads REQUIRED)

message("boost header: ${BOOST_INC_DIR}")

set(ImgFullPipeGUITestMulti_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE ImgFullPipeGUITestMulti_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)

add_executable(ImgFullPipeGUITestMulti "${ImgFullPipeGUITestMulti_SRC}")
target_include_directories(ImgFullPipeGUITestMulti PUBLIC "$<BUILD_INTERFACE:${ImgFullPipeGUITestMulti_INC_DIR}>")

target_link_libraries(ImgFullPipeGUITestMulti Threads::Threads dl)

target_include_directories(ImgFullPipeGUITestMulti PUBLIC ${Boost_INCLUDE_DIR})
