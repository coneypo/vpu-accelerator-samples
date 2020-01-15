cmake_policy(SET CMP0015 NEW)

get_filename_component(pluginname ${CMAKE_CURRENT_SOURCE_DIR} NAME)
set(pluginname gst${pluginname})

configure_file ("${PLUGIN_ROOT_DIR}/gst-lib/ocl/oclcommon.h.in"
                "${CMAKE_BINARY_DIR}/ocl/oclcommon.h")

include(${CMAKE_SOURCE_DIR}/cmake/FindMediaSDK.cmake)

aux_source_directory(${PLUGIN_ROOT_DIR}/gst-lib/ocl DIR_SRCS)
aux_source_directory(${PLUGIN_ROOT_DIR}/gst-lib/algo DIR_SRCS)
aux_source_directory(. DIR_SRCS)

add_library (${pluginname} SHARED ${DIR_SRCS})

target_include_directories(${pluginname} PRIVATE .
                           ${MFX_INCLUDES}
                           ${CMAKE_BINARY_DIR}/ocl
                           ${CMAKE_SOURCE_DIR}/utils/
                           ${PLUGIN_ROOT_DIR}/gst-lib/
                           ${PLUGIN_ROOT_DIR}/gst-lib/inferresultmeta
                           ${PLUGIN_ROOT_DIR}/gst-lib/ocl
                           ${PLUGIN_ROOT_DIR}/gst-lib/algo
                           ${PLUGIN_ROOT_DIR}/gst-lib/interface)

target_link_directories(${pluginname} PRIVATE ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

target_link_libraries(${pluginname} PRIVATE
    ${OpenCV_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GLIB2_LIBRARIES}
    inferresultmeta
    OpenCL
    gstmfx msdkmfx)

add_dependencies(${pluginname} msdk-gst)

message(STATUS "Copy OpenCL kernel to: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")

file(COPY ../../gst-lib/ocl/kernels
     DESTINATION ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

install(TARGETS ${pluginname} DESTINATION lib)