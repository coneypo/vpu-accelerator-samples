get_filename_component(pluginname ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(pluginname gst${pluginname})

aux_source_directory(. DIR_SRCS)


add_library(${pluginname} SHARED ${DIR_SRCS})

target_include_directories(${pluginname} PRIVATE .
        ${GSTAPP_INCLUDE_DIRS}
        ${PLUGIN_ROOT_DIR}/gst-lib)

target_link_libraries(${pluginname}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GSTAPP_LIBRARIES}
        ${GLIB2_LIBRARIES}
        utils_static
        )

install(TARGETS ${pluginname} DESTINATION lib)
