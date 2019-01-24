find_package(XLinkFake REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRC PROTO_HDR hal_message.proto ../message/hddl_message.proto)

# Add this extra line to make CLion analyze better though it's already added as target porperty
include_directories(${CMAKE_CURRENT_BINARY_DIR})

file(GLOB SRC "*.cpp")
file(GLOB HDR "*.h")

add_executable(hddl_manager ${SRC} ${HDR} ${PROTO_SRC} ${PROTO_HDR})
target_compile_options(hddl_manager PRIVATE -std=c++11 -Wall -pthread)
target_compile_definitions(hddl_manager PRIVATE -DINSTALL_DIR=${CMAKE_INSTALL_PREFIX})
target_include_directories(hddl_manager PRIVATE
    $<BUILD_INTERFACE:${PROTOBUF_INCLUDE_DIR}>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
target_link_libraries(hddl_manager pthread XLinkFake::xlinkfake_arm ${Boost_LIBRARIES} ${PROTOBUF_LITE_LIBRARY})
