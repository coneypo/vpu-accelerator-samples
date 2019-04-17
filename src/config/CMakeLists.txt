set(TARGET_NAME mp_config_static)

file(GLOB HDRS "*.h")
aux_source_directory(. SRCS)
add_library(${TARGET_NAME} STATIC ${HDRS} ${SRCS})

target_include_directories(
    ${TARGET_NAME} PUBLIC
    ${GLIB_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(
    ${TARGET_NAME} PRIVATE
    mp_utils_static
    ${JSON-C_LIBRARIES}
)