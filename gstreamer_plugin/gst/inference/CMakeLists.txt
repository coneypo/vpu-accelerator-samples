get_filename_component(pluginname ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(pluginname gst${pluginname})

aux_source_directory(. DIR_SRCS)
aux_source_directory(${PLUGIN_ROOT_DIR}/gst-lib/inferresultmeta DIR_SRCS)

find_package(Boost REQUIRED system filesystem)

add_library(${pluginname} SHARED ${DIR_SRCS})
target_include_directories(${pluginname} PRIVATE .
        ${PLUGIN_ROOT_DIR}/gst-lib)

target_include_directories(${pluginname} PUBLIC
        ${PLUGIN_ROOT_DIR}/gst-lib/inferresultmeta)


target_link_libraries(${pluginname}
        ${OpenCV_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GLIB2_LIBRARIES}
        ${Boost_LIBRARIES}
        utils_static
        )

install(TARGETS ${pluginname} DESTINATION lib)
