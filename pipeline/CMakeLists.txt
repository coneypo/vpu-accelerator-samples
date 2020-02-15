set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(${QT_DEFINITIONS})

aux_source_directory(. DIR_SRCS)

set(TARGET_NAME hddlpipeline)
add_executable(${TARGET_NAME} ${DIR_SRCS})
add_dependencies(${TARGET_NAME} utils_static)

target_include_directories(${TARGET_NAME} PRIVATE
        ${GSTREAMER_INCLUDE_DIRS}
        ${GSTVIDEO_INCLUDE_DIRS}
        ${OpenCV_INCLUDE_DIRS}
        ${Qt5Widgets_INCLUDE_DIR}
        ${Qt5Network_INCLUDE_DIR})

target_link_libraries(${TARGET_NAME} PRIVATE
        utils_static
        ${OpenCV_LIBRARIES}
        ${Qt5Widgets_LIBRARIES}
        ${Qt5Network_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${CMAKE_THREAD_LIBS_INIT})

install(TARGETS ${TARGET_NAME} DESTINATION bin)
