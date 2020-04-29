get_filename_component(pluginname ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(pluginname gst${pluginname})

include(${CMAKE_SOURCE_DIR}/cmake/FindMediaSDK.cmake)

aux_source_directory(${PLUGIN_ROOT_DIR}/gst-lib/osdparser DIR_SRCS)
aux_source_directory(. DIR_SRCS)

add_library(${pluginname} SHARED ${DIR_SRCS})
target_compile_options(${pluginname} PRIVATE -Wall -Wextra -Werror -fPIC -shared -fstack-protector-strong -Wformat -Wformat-security -Werror=format-security -fno-strict-overflow  -fno-delete-null-pointer-checks -fwrapv -D_FORTIFY_SOURCE=2)

target_include_directories(${pluginname} PRIVATE .
        ${MFX_INCLUDES}
        ${OpenCV_INCLUDE_DIRS}
        ${MSDK_GST_SOURCE}
        ${MSDK_GST_SOURCE}/gst/mfx
        ${MSDK_GST_SOURCE}/gst-libs/mfx
        ${MSDK_GST_BINARY}
        ${PLUGIN_ROOT_DIR}/gst-lib/osdparser
        ${PLUGIN_ROOT_DIR}/gst-lib/inferresultmeta
        ${PLUGIN_ROOT_DIR}/gst-lib/common)

target_link_libraries(${pluginname}
        ${OpenCV_LIBRARIES}
        ${GSTREAMER_LIBRARIES}
        ${GSTVIDEO_LIBRARIES}
        ${GLIB2_LIBRARIES}
        gstinference
        hddldemoutils
        ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/libgstmfx.so)

add_dependencies(${pluginname} msdk-gst hddldemoutils)

install(TARGETS ${pluginname} DESTINATION lib)
