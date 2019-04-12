find_package(XLink REQUIRED)
find_package(OpenCV REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(Protobuf REQUIRED)

protobuf_generate_cpp(PROTO_SRC PROTO_HDR hal_message.proto)

# Add this extra line to make CLion analyze better though it's already added as target porperty
include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(SRC main.cpp XLinkConnector.cpp PipelineManager.cpp Pipeline.cpp PipelineState.cpp)
set(HDR XLinkConnector.h PipelineManager.h PipelineStatus.h Pipeline.h PipelineState.h)

if(MULTI_THREAD_MODE)
    mediapipe_library_build(NAME hddl_manager_lib MODULES element openvino mix2)
    target_compile_definitions(hddl_manager_lib PRIVATE $<$<CONFIG:Debug>:DEBUG>)
    target_include_directories(hddl_manager_lib PRIVATE ${OpenCV_INCLUDE_DIR})
    target_link_libraries(hddl_manager_lib PRIVATE ${OpenCV_LIBRARIES})

    list(APPEND SRC PipelineThread.cpp)
    add_executable(hddl_manager ${SRC} ${HDR} ${PROTO_SRC} ${PROTO_HDR} ${SRC_LIST})
    target_link_libraries(hddl_manager hddl_manager_lib)
else()
    protobuf_generate_cpp(PROTO_PROC_SRC PROTO_PROC_HDR ../message/hddl_message.proto)

    list(APPEND SRC PipelineProcess.cpp PipelineIPC.cpp SubProcess.cpp PipelineIpcClient.cpp)
    list(APPEND HDR PipelineIPC.h SubProcess.h PipelineIpcClient.h)
    add_executable(hddl_manager ${SRC} ${HDR} ${PROTO_SRC} ${PROTO_HDR} ${PROTO_PROC_SRC} ${PROTO_PROC_HDR})

    target_compile_definitions(hddl_manager PRIVATE -DINSTALL_DIR=${CMAKE_INSTALL_PREFIX})
endif()

target_compile_options(hddl_manager PRIVATE -std=c++11 -Wall -pthread)
target_include_directories(hddl_manager PRIVATE
        $<BUILD_INTERFACE:${PROTOBUF_INCLUDE_DIR}>
        $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>)
target_link_libraries(hddl_manager pthread XLink::XLinkARM ${Boost_LIBRARIES} ${PROTOBUF_LITE_LIBRARY})

install(TARGETS hddl_manager
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)
