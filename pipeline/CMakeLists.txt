set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(${QT_DEFINITIONS})

aux_source_directory(. DIR_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/common UTILS_SRCS)

set(TARGET_NAME hddlpipeline)
add_executable(${TARGET_NAME} ${DIR_SRCS} ${UTILS_SRCS})
target_include_directories(${TARGET_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
    ${Qt5Network_INCLUDE_DIR}
    ${Qt5Widgets_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ${CMAKE_SOURCE_DIR}/common
    )

target_link_libraries(${TARGET_NAME}
    utils_static
    ${Boost_LIBRARIES}
    ${OpenCV_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${Qt5Widgets_LIBRARIES})

install(TARGETS ${TARGET_NAME} DESTINATION bin)
