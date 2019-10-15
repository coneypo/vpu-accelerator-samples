# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

mediapipe_library_build(NAME hddl_mediapipe2_lib MODULES element openvino metadata)

file(GLOB_RECURSE HDRS "*.h")
file(GLOB_RECURSE SRCS "*.c")

include_directories(${CMAKE_SOURCE_DIR}/front/hddl_mediapipe)
include_directories(${CMAKE_SOURCE_DIR}/front/hddl_mediapipe/process_command)

add_executable(hddl_mediapipe2 ${SRCS} ${HDRS})
target_link_libraries(hddl_mediapipe2 hddl_mediapipe2_lib pthread)

#set FOLDER for subdir install
set_property(TARGET hddl_mediapipe2 PROPERTY FOLDER "executables")

install(TARGETS hddl_mediapipe2 DESTINATION /usr/bin)

#copy config.json and launch.txt into make dir
configure_file(./config_openvino_filesrc.json  ${CMAKE_CURRENT_BINARY_DIR}/config_openvino_filesrc.json COPYONLY)
configure_file(./launch_openvino_filesrc.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_openvino_filesrc.txt COPYONLY)
configure_file(./config_openvino_new_plugins.json  ${CMAKE_CURRENT_BINARY_DIR}/config_openvino_new_plugins.json COPYONLY)
configure_file(./launch_openvino_new_plugins.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_openvino_new_plugins.txt COPYONLY)
configure_file(./config_openvino_send_metadata_new_plugins.json  ${CMAKE_CURRENT_BINARY_DIR}/config_openvino_send_metadata_new_plugins.json COPYONLY)
configure_file(./launch_openvino_send_metadata_new_plugins.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_openvino_send_metadata_new_plugins.txt COPYONLY)
configure_file(./labels/detect_person_vehicle_bike.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/detect_person_vehicle_bike.txt COPYONLY)
configure_file(./labels/pedestrian_attributes.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/pedestrian_attributes.txt COPYONLY)
configure_file(./labels/vehicle_types.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/vehicle_types.txt COPYONLY)
configure_file(./labels/vehicle_colors.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/vehicle_colors.txt COPYONLY)

