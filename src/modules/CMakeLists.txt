set(TARGET_NAME mp_modules_static)

add_subdirectory(meta)

file(GLOB HDRS "*.h")
aux_source_directory(. SRCS)
list(REMOVE_ITEM SRCS ./mp_crop.c ./mp_mix.c ./mp_metadata.c)

# set some module compiled by CXX
set_source_files_properties(
    ./mp_gva_postproc_and_upload.c
    ./mp_feeder.c
    ./mp_mix2.c
    ./mp_metadata.c
    ./mp_openvino.c
    ./mp_metaparser.c
    ./mp_dump_buffer.c
    PROPERTIES LANGUAGE CXX
)

set(MODULE_COLLECTION ${CMAKE_BINARY_DIR}/mp_module_collection.c)
generate_module_collection_file(MODULE_FILES ${SRCS} OUTPUT ${MODULE_COLLECTION})

add_library(${TARGET_NAME} STATIC ${HDRS} ${SRCS} ${MODULE_COLLECTION})

find_library (HDDL_PLUGIN_LIBRARY
    NAMES gsthddl
    PATHS /usr/local/lib/gstreamer-1.0/
    )

if(HDDL_PLUGIN_LIBRARY)
    message(STATUS "Find gsthddl library success")
else()
    message(WARNING "Find gsthddl library faild")
endif()

target_include_directories(
    ${TARGET_NAME} PUBLIC
    ${GLIB_INCLUDE_DIRS}
    ${CAIRO_INCLUDE_DIRS}
    ${OpenCV_INCLUDE_DIR}
    ${LIBPANGO_INCLUDE_DIRS}
    ${GSTREAMER_INCLUDE_DIRS}
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${PROJECT_SOURCE_DIR}/src/core
    ${PROJECT_SOURCE_DIR}/front/hddl_mediapipe
    ${PROJECT_SOURCE_DIR}/front/hddl_mediapipe/process_command
)

target_link_libraries(
    ${TARGET_NAME} PRIVATE
    mp_meta_static
    mp_utils_static
    ${Boost_LIBRARIES}
    ${CAIRO_LIBRARIES}
    ${OpenCV_LIBRARIES}
    ${HDDL_PLUGIN_LIBRARY}
    ${LIBPANGO_CAIRO_LIBRARY}
    ${GSTREAMER_VIDEO_LIBRARIES}
    ${GSTREAMER_RTSPSERVER_LIBRARIES}
)

target_compile_options(
    ${TARGET_NAME} PUBLIC
    $<$<COMPILE_LANGUAGE:C>:-std=gnu99>
    $<$<COMPILE_LANGUAGE:CXX>:-std=c++11>
    -Wno-deprecated-declarations -Werror
)

target_compile_definitions(
    ${TARGET_NAME} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

if(CROP_NV12)
    target_compile_definitions(${TARGET_NAME} PRIVATE CROP_NV12)
endif()