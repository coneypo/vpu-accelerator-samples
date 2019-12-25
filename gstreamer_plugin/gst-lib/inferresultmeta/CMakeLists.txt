get_filename_component(libname ${CMAKE_CURRENT_SOURCE_DIR} NAME)

aux_source_directory(. DIR_SRCS)

add_library (${libname} SHARED ${DIR_SRCS})

target_include_directories(${libname} PRIVATE ..)
target_link_libraries(${libname}
    ${GSTREAMER_LIBRARIES}
    ${GSTVIDEO_LIBRARIES}
    ${GLIB2_LIBRARIES}
    )

install(TARGETS ${libname} DESTINATION lib)
