set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

add_definitions(-DQT_NO_KEYWORDS)
add_definitions(${QT_DEFINITIONS})


aux_source_directory(. DIR_SRCS)
aux_source_directory(${CMAKE_SOURCE_DIR}/common UTILS_SRCS)
qt5_wrap_ui(UI_SRC hddldemo.ui)

add_executable(${PROJECT_NAME} ${DIR_SRCS} ${UI_SRC} ${UTILS_SRCS})
target_link_libraries(${PROJECT_NAME} PRIVATE common_static)

install(TARGETS ${PROJECT_NAME} DESTINATION bin)
install(FILES "config.txt" DESTINATION "./")
