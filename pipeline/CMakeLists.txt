set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(${QT_DEFINITIONS})

aux_source_directory(. DIR_SRCS)

set(TARGET_NAME hddlpipeline)
add_executable(${TARGET_NAME} ${DIR_SRCS})
target_include_directories(${TARGET_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
        )

target_link_libraries(${TARGET_NAME}
        utils_static
        common_static
        ${Boost_LIBRARIES}
        ${OpenCV_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        )

install(TARGETS ${TARGET_NAME} DESTINATION bin)
