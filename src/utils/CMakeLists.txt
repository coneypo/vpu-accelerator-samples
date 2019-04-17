set(TARGET_NAME mp_utils_static)

file(GLOB HDRS "*.h")
aux_source_directory(. SRCS)
add_library(${TARGET_NAME} STATIC ${HDRS} ${SRCS})

target_include_directories(
    ${TARGET_NAME} PUBLIC
    ${GLIB_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${LIBPANGO_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ${TARGET_NAME} PRIVATE
    ${GLIB_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${LIBPANGO_LIBRARIES}
    ${GSTREAMER_LIBRARIES}
    ${GLIB_GOBJECT_LIBRARIES}
)