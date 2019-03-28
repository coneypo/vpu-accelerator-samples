find_package(OpenCV REQUIRED)

mediapipe_library_build(NAME mulclientlib MODULES teststream crop_va)
target_include_directories(mulclientlib PRIVATE ${OpenCV_INCLUDE_DIR})
target_link_libraries(mulclientlib PRIVATE ${OpenCV_LIBRARIES})

set(TargetName multi_client)

add_executable(${TargetName} main.c)
target_link_libraries(${TargetName} PRIVATE mulclientlib XLinkARM pthread)
set_target_properties(${TargetName} PROPERTIES FOLDER "executables" LINKER_LANGUAGE CXX)
target_compile_definitions(${TargetName} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

configure_file(./launch_test_stream.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_test_stream.txt COPYONLY)
configure_file(./config_test_stream.json  ${CMAKE_CURRENT_BINARY_DIR}/config_test_stream.json COPYONLY)

