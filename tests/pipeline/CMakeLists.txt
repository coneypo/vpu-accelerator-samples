set(CMAKE_INCLUDE_CURRENT_DIR ON)

aux_source_directory(. DIR_SRCS)
aux_source_directory(../common UTILS_SRCS)

set(TARGET_NAME pipeline_test)
add_executable(${TARGET_NAME} ${DIR_SRCS} ${UTILS_SRCS})
add_dependencies(${TARGET_NAME} msdk-gst inferresultmeta)
target_include_directories(${TARGET_NAME} PRIVATE ${GSTREAMER_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
    ${GSTAPP_INCLUDE_DIRS}
    ../common
    )

target_link_directories(${TARGET_NAME} PRIVATE
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gstreamer-1.0
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

target_link_libraries(${TARGET_NAME}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${XcbDri_LIBRARIES}
    )

install(TARGETS ${TARGET_NAME} DESTINATION bin)
