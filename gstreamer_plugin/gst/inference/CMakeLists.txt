get_filename_component(pluginname ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(pluginname gst${pluginname})

aux_source_directory(. DIR_SRCS)

find_package(Boost REQUIRED system filesystem)
message(STATUS "boost library: " ${Boost_LIBRARIES})

add_library (${pluginname} SHARED ${DIR_SRCS})
target_include_directories (${pluginname} PRIVATE .
                            ${PLUGIN_ROOT_DIR}/gst-lib
                            ${PLUGIN_ROOT_DIR}/gst-lib/inferresultmeta)

target_link_libraries(${pluginname}
    ${OpenCV_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GLIB2_LIBRARIES}
    inferresultmeta
    ${Boost_LIBRARIES}
    utils_static
    )

install(TARGETS ${pluginname} DESTINATION lib)
