# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

mediapipe_library_build(NAME mediapipe2_lib MODULES element rtsp changeformat onvif openvino mix2 encoder openvino_tracking mix3 openssl_hw encrypt)

add_definitions(-DAPP_IPC)

#file(GLOB_RECURSE HDRS "*.h")
#file(GLOB_RECURSE SRCS "*.c")

include_directories(${CMAKE_SOURCE_DIR}/front/ipc_mediapipe)

#add_executable(mediapipe2 ${SRCS} ${HDRS})

add_executable(mediapipe2 ./main.c)
target_link_libraries(mediapipe2 mediapipe2_lib pthread)

#set FOLDER for subdir install
set_property(TARGET mediapipe2 PROPERTY FOLDER "executables")

install(TARGETS mediapipe2 DESTINATION /usr/bin)
install(FILES ./config.json DESTINATION /etc/mediapipe2)
install(FILES ./launch.txt DESTINATION /etc/mediapipe2)
install(FILES ./config_no_detect.json DESTINATION /etc/mediapipe2)
install(FILES ./launch_no_audio.txt DESTINATION /etc/mediapipe2)
install(FILES ./launch_one_channel.txt DESTINATION /etc/mediapipe2)

#copy config.json and launch.txt into make dir
configure_file(./config.json  ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
configure_file(./launch.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch.txt COPYONLY)
configure_file(./config_no_detect.json  ${CMAKE_CURRENT_BINARY_DIR}/config_no_detect.json COPYONLY)
configure_file(./launch_no_audio.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_no_audio.txt COPYONLY)
configure_file(./launch_one_channel.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_one_channel.txt COPYONLY)
configure_file(./config_onvif.json  ${CMAKE_CURRENT_BINARY_DIR}/config_onvif.json COPYONLY)
configure_file(./launch_onvif.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_onvif.txt COPYONLY)
configure_file(./multi_mix_cvsdk_config.json  ${CMAKE_CURRENT_BINARY_DIR}/multi_mix_cvsdk_config.json COPYONLY)
configure_file(./multi_mix_cvsdk_launch.txt  ${CMAKE_CURRENT_BINARY_DIR}/multi_mix_cvsdk_launch.txt COPYONLY)
configure_file(./decode_config.json  ${CMAKE_CURRENT_BINARY_DIR}/decode_config.json COPYONLY)
configure_file(./decode_launch.txt  ${CMAKE_CURRENT_BINARY_DIR}/decode_launch.txt COPYONLY)
configure_file(./config_testbuffer.json  ${CMAKE_CURRENT_BINARY_DIR}/config_testbuffer.json COPYONLY)
configure_file(./launch_testbuffer.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_testbuffer.txt COPYONLY)
configure_file(./config_openvino_new.json  ${CMAKE_CURRENT_BINARY_DIR}/config_openvino_new.json COPYONLY)
configure_file(./launch_openvino_new.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_openvino_new.txt COPYONLY)
configure_file(./labels/detect_person_vehicle_bike.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/detect_person_vehicle_bike.txt COPYONLY)
configure_file(./labels/pedestrian_attributes.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/pedestrian_attributes.txt COPYONLY)
configure_file(./labels/vehicle_types.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/vehicle_types.txt COPYONLY)
configure_file(./labels/vehicle_colors.txt  ${CMAKE_CURRENT_BINARY_DIR}/labels/vehicle_colors.txt COPYONLY)

option(BUILD_IPC_DEMO "Build IPC Demo" OFF)
if(BUILD_IPC_DEMO)
    add_subdirectory(demo)
endif()

