find_package(Boost 1.53 COMPONENTS filesystem REQUIRED)

find_package(Threads REQUIRED)

message("boost header: ${BOOST_INC_DIR}")

set(FULL_PIPE_GUI_TEST_SOCK_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE FULL_PIPE_GUI_TEST_SOCK_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)

add_executable(FullPipeGUITestSock "${FULL_PIPE_GUI_TEST_SOCK_SRC}")
target_include_directories(FullPipeGUITestSock PUBLIC "$<BUILD_INTERFACE:${FULL_PIPE_GUI_TEST_SOCK_INC_DIR}>")

target_link_libraries(FullPipeGUITestSock Threads::Threads dl)

target_include_directories(FullPipeGUITestSock PUBLIC ${Boost_INCLUDE_DIR})