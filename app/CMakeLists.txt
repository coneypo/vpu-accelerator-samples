set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(${QT_DEFINITIONS})

aux_source_directory(. DIR_SRCS)
qt5_wrap_ui(UI_SRC hddldemo.ui)

add_executable(${PROJECT_NAME} ${DIR_SRCS} ${UI_SRC})
target_include_directories(${PROJECT_NAME} PRIVATE
        ${Qt5Widgets_INCLUDE_DIR}
        ${Qt5Network_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} PRIVATE
        ${Qt5Widgets_LIBRARIES}
        ${Qt5Network_LIBRARIES}
        utils_static)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES "config.txt" DESTINATION "./")
