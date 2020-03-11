find_package(Boost 1.53 COMPONENTS filesystem REQUIRED)

find_package(Threads REQUIRED)

message("boost header: ${BOOST_INC_DIR}")

set(GUI_IPC_TEST_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE GUI_IPC_TEST_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)

add_executable(FullPipeGUITest "${GUI_IPC_TEST_SRC}")
target_include_directories(FullPipeGUITest PUBLIC "$<BUILD_INTERFACE:${GUI_IPC_TEST_INC_DIR}>")

target_link_libraries(FullPipeGUITest Threads::Threads dl)

target_include_directories(FullPipeGUITest PUBLIC ${Boost_INCLUDE_DIR})