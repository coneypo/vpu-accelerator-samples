include(CMakeParseArguments)

#defind module_path
set(BASE_MODULES_PATH ${CMAKE_SOURCE_DIR}/src/modules)

#add package find path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/" "${BASE_MODULES_PATH}/cmake/modules")

find_package(Json-c REQUIRED)
find_package(Cairo REQUIRED)
find_package(Glib REQUIRED)
find_package(Gstreamer REQUIRED)
find_package(Pango REQUIRED)

function(mediapipe_library_build)
    set(singleValueArgs NAME)
    set(multiValueArgs MODULES)
    cmake_parse_arguments(MEDIAPIPE "" "${singleValueArgs}" "${multiValueArgs}" ${ARGN})

    if(NOT DEFINED MEDIAPIPE_NAME)
        message(FATAL_ERROR "mediapipe_build NAME is not set")
    endif()

    #define code complile file
    file(GLOB_RECURSE SRC_LIST "${CMAKE_SOURCE_DIR}/src/config/*.c" "${CMAKE_SOURCE_DIR}/src/core/*.c" "${CMAKE_SOURCE_DIR}/src/utils/*.c" )

    if(DEFINED MEDIAPIPE_MODULES)
        #define modules that will be compliled
        set(MP_MODULES ${MEDIAPIPE_MODULES})
    endif()

    #add modules source file
    foreach(_module ${MP_MODULES})
        list(APPEND SRC_LIST "${BASE_MODULES_PATH}/mp_${_module}.c")
    endforeach(_module)

    #add addon moudle
    set(ADDON_MODULE_PATH "" CACHE STRING "addon modules path")
    message("${ADDON_MODULE_PATH}")
    if (NOT ${ADDON_MODULE_PATH} STREQUAL "")
        foreach(_module_path ${ADDON_MODULE_PATH})
            if(EXISTS ${_module_path}/CMakeLists.txt)
                include(${_module_path}/CMakeLists.txt)
            else(EXISTS ${_module_path}/CMakeLists.txt)
                message( FATAL_ERROR "${_module_path}/CMakeLists.txt not exist" )
            endif(EXISTS ${_module_path}/CMakeLists.txt)
        endforeach(_module_path ${ADDON_MODULE_PATH})
    endif()

    #create c code file that contains all select moudles
    set(modulecfile ${CMAKE_CURRENT_BINARY_DIR}/mp_modules.c)
    file(WRITE ${modulecfile} "#include \"mp_module.h\"\n")
    foreach(_module ${MP_MODULES})
        file(APPEND ${modulecfile} "extern mp_module_t  mp_${_module}_module;\n")
    endforeach(_module)
    file(APPEND ${modulecfile} "\n")
    file(APPEND ${modulecfile} "mp_module_t *mp_modules[] = {\n")
    foreach(_module ${MP_MODULES})
        file(APPEND ${modulecfile} "\t&mp_${_module}_module,\n")
    endforeach(_module)
    file(APPEND ${modulecfile} "\tNULL\n")
    file(APPEND ${modulecfile} "};\n")
    file(APPEND ${modulecfile} "\n")
    file(APPEND ${modulecfile} "char *mp_module_names[] = { \n")
    foreach(_module ${MP_MODULES})
        file(APPEND ${modulecfile} "\t\"mp_${_module}_module\", \n")
    endforeach(_module)
    file(APPEND ${modulecfile} "\tNULL\n")
    file(APPEND ${modulecfile} "};\n")
    file(APPEND ${modulecfile} "\n")

    list(APPEND SRC_LIST ${modulecfile})

    # set some module compiled by CXX
    set_source_files_properties(${BASE_MODULES_PATH}/mp_crop.c ${BASE_MODULES_PATH}/mp_mix.c ${BASE_MODULES_PATH}/mp_mix2.c ${BASE_MODULES_PATH}/mp_metadata.c PROPERTIES LANGUAGE CXX)
    set_source_files_properties(${BASE_MODULES_PATH}/mp_openvino.c PROPERTIES LANGUAGE CXX)

    add_library(${MEDIAPIPE_NAME} STATIC ${SRC_LIST})

    target_compile_options(${MEDIAPIPE_NAME} PUBLIC $<$<COMPILE_LANGUAGE:C>:-std=gnu99> $<$<COMPILE_LANGUAGE:CXX>:-std=c++11> -Wno-deprecated-declarations -Werror)
    target_compile_definitions(${MEDIAPIPE_NAME} PUBLIC $<$<CONFIG:Debug>:DEBUG>)

    #opencl oclcommon.h didn't not add extern c heard, so there is some problem
    #when link libgstocl.so, so force use CXX link
    set_target_properties(${MEDIAPIPE_NAME} PROPERTIES LINKER_LANGUAGE CXX)

    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${JSON-C_INCLUDE_DIRS})
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${CAIRO_INCLUDE_DIRS})
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${GLIB_INCLUDE_DIRS})
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${GSTREAMER_INCLUDE_DIRS})
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${LIBPANGO_INCLUDE_DIRS})
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/config)
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/core)
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/meta)
    target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/utils)

    target_link_libraries(${MEDIAPIPE_NAME} ${JSON-C_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${GLIB_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${GLIB_GOBJECT_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${GSTREAMER_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${GSTREAMER_BASE_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${GSTREAMER_APP_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${GSTREAMER_VIDEO_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${GSTREAMER_RTSPSERVER_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${CAIRO_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${LIBPANGO_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} ${LIBPANGO_CAIRO_LIBRARY})
    target_link_libraries(${MEDIAPIPE_NAME} ${ADDON_MODULES_DEP_LIBS})
endfunction()
