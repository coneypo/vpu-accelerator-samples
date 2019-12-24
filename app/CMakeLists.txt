find_package(Threads)
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)


set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(${QT_DEFINITIONS})
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${QTGSTREAMER_FLAGS}")

aux_source_directory(. DIR_SRCS)
aux_source_directory(../utils_app UTILS_SRCS)
qt5_wrap_ui(UI_SRC hddldemo.ui)

add_executable(${PROJECT_NAME} ${DIR_SRCS} ${UI_SRC} ${UTILS_SRCS})
target_include_directories(${PROJECT_NAME} PRIVATE
                           ${GSTREAMER_INCLUDE_DIRS}
                           ${Qt5Widgets_INCLUDE_DIR}
                           ${Qt5Network_INCLUDE_DIR}
                           ${CMAKE_SOURCE_DIR}/utils_app)
target_link_libraries(${PROJECT_NAME}
    ${CMAKE_THREAD_LIBS_INIT}
    ${GSTREAMER_LIBRARIES}
    ${Qt5Widgets_LIBRARIES}
    ${Qt5Network_LIBRARIES})
