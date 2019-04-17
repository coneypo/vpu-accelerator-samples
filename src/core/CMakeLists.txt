set(TARGET_NAME mp_core_static)

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
    mp_utils_static
    mp_config_static
    mp_modules_static
    ${GSTREAMER_LIBRARIES}
    ${GSTREAMER_APP_LIBRARIES}
    ${GSTREAMER_BASE_LIBRARIES}
)

target_compile_options(
    ${TARGET_NAME} PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++11>
    -Wno-deprecated-declarations -Werror
)