find_package(Boost 1.53 COMPONENTS filesystem REQUIRED)

find_package(Threads REQUIRED)

message("boost header: ${BOOST_INC_DIR}")

set(FullPipeGUITestMulti_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE FullPipeGUITestMulti_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)

add_executable(FullPipeGUITestMulti "${FullPipeGUITestMulti_SRC}")
target_include_directories(FullPipeGUITestMulti PUBLIC "$<BUILD_INTERFACE:${FullPipeGUITestMulti_INC_DIR}>")

target_link_libraries(FullPipeGUITestMulti Threads::Threads dl)

target_include_directories(FullPipeGUITestMulti PUBLIC ${Boost_INCLUDE_DIR})