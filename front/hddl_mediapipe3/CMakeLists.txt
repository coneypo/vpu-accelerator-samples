mediapipe_library_build(NAME hddl_mediapipe3_lib MODULES element openvino mix2)

find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRC PROTO_HDR ../message/hddl_message.proto)

file(GLOB HDRS "*.h")
file(GLOB SRCS "*.c")

include_directories(${CMAKE_SOURCE_DIR}/front/hddl_mediapipe3)
include_directories(${PROTOBUF_INCLUDE_DIR})
include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(hddl_mediapipe3 ${SRCS} ${HDRS} message.cpp ${PROTO_SRC} ${PROTO_HDR})
target_link_libraries(hddl_mediapipe3 hddl_mediapipe3_lib pthread ${PROTOBUF_LITE_LIBRARY})

#set FOLDER for subdir install
set_property(TARGET hddl_mediapipe3 PROPERTY FOLDER "executables")

install(TARGETS hddl_mediapipe3
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
