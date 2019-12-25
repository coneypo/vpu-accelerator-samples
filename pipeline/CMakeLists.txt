find_package(Threads)
find_package(Qt5 COMPONENTS Core Gui Widgets Network REQUIRED)

pkg_check_modules(GSTREAMER  REQUIRED  gstreamer-1.0)
pkg_check_modules(GSTVIDEO  REQUIRED  gstreamer-video-1.0)
pkg_check_modules(GSTAPP  REQUIRED  gstreamer-app-1.0)
pkg_check_modules(OpenCV     REQUIRED  opencv4)
pkg_check_modules(XcbDri     REQUIRED  xcb-dri3)

include(${CMAKE_SOURCE_DIR}/cmake/FindMediaSDK.cmake)

add_definitions(-DQT_NO_KEYWORDS)
set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

aux_source_directory(. DIR_SRCS)
aux_source_directory(../common UTILS_SRCS)

add_executable(hddlpipeline ${DIR_SRCS} ${UTILS_SRCS})
add_dependencies(hddlpipeline msdk-gst inferresultmeta)
target_include_directories(hddlpipeline PRIVATE ${GSTREAMER_INCLUDE_DIRS}
    ${GSTVIDEO_INCLUDE_DIRS}
    ${GSTAPP_INCLUDE_DIRS}
    ${Qt5Network_INCLUDE_DIR}
    ${Qt5Widgets_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIRS}
    ../common
    ../gstreamer_plugin/gst/displaysink
    ../gstreamer_plugin/gst-lib/
    ../gstreamer_plugin/gst-lib/inferresultmeta
    ${MSDK_GST_SOURCE}
    ${MSDK_GST_SOURCE}/gst/mfx
    ${MSDK_GST_SOURCE}/gst-libs/mfx
    ${MSDK_GST_BINARY}
    )

target_link_directories(hddlpipeline PRIVATE
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY}/gstreamer-1.0
    ${CMAKE_LIBRARY_OUTPUT_DIRECTORY})

target_link_libraries(hddlpipeline
    gstmfx
    inferresultmeta
    gstinference
    ${CMAKE_THREAD_LIBS_INIT}
    ${OpenCV_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GSTAPP_LIBRARIES}
    ${XcbDri_LIBRARIES}
    ${Qt5Network_LIBRARIES}
    ${Qt5Widgets_LIBRARIES})
