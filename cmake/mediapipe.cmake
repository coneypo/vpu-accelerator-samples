# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

include(CMakeParseArguments)

find_library (HDDL_PLUGIN_LIBRARY
    NAMES gsthddl
    PATHS
        /usr/lib/gstreamer-1.0/
        /usr/local/lib/gstreamer-1.0/
    )

if(HDDL_PLUGIN_LIBRARY)
    message(STATUS "Found gsthddl library: ${HDDL_PLUGIN_LIBRARY}")
else()
    message(WARNING "No libgsthddl.so found in '/usr/lib/gstreamer-1.0/' or '/usr/local/lib/gstreamer-1.0/'")
endif()

# create c code file that contains all modules
function(generate_module_collection_file)
    set(options)
    set(args OUTPUT)
    set(list_args MODULE_FILES)

    cmake_parse_arguments(
        generator
        "${options}"
        "${args}"
        "${list_args}"
        ${ARGN}
    )

    set(MODULE_FILES ${generator_MODULE_FILES})
    set(MODULE_COLLECTION ${generator_OUTPUT})

    foreach(FILE ${MODULE_FILES})
        get_filename_component(MODULE ${FILE} NAME_WE)
        list(APPEND MODULES ${MODULE})
    endforeach()

    list(SORT MODULES)

    file(WRITE ${MODULE_COLLECTION} "#include \"mp_module.h\"\n\n")

    foreach(ITEM ${MODULES})
        file(APPEND ${MODULE_COLLECTION} "extern mp_module_t ${ITEM}_module;\n")
    endforeach()

    file(APPEND ${MODULE_COLLECTION} "\nmp_module_t *mp_modules[] = {\n")
    foreach(ITEM ${MODULES})
        file(APPEND ${MODULE_COLLECTION} "\t&${ITEM}_module,\n")
    endforeach()
    file(APPEND ${MODULE_COLLECTION} "\tNULL\n")
    file(APPEND ${MODULE_COLLECTION} "};\n\n")

    file(APPEND ${MODULE_COLLECTION} "char* mp_module_names[] = { \n")
    foreach(ITEM ${MODULES})
        file(APPEND ${MODULE_COLLECTION} "\t\"${ITEM}_module\", \n")
    endforeach()
    file(APPEND ${MODULE_COLLECTION} "\tNULL\n")
    file(APPEND ${MODULE_COLLECTION} "};\n")
    file(APPEND ${MODULE_COLLECTION} "\n")

    message(STATUS "Generate ${MODULE_COLLECTION} done")
endfunction()


function(mediapipe_library_build)
    #defind module_path
    set(BASE_MODULES_PATH ${CMAKE_SOURCE_DIR}/src/modules)

    #add package find path
    set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/" "${BASE_MODULES_PATH}/cmake/modules")

    find_package(Json-c REQUIRED)
    find_package(Cairo REQUIRED)
    find_package(Glib REQUIRED)
    find_package(Gstreamer REQUIRED)
    find_package(Pango REQUIRED)

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
        if(EXISTS "${BASE_MODULES_PATH}/mp_${_module}.c")
            list(APPEND SRC_LIST "${BASE_MODULES_PATH}/mp_${_module}.c")
        elseif(EXISTS "${CMAKE_CURRENT_LIST_DIR}/mp_${_module}.c")
            list(APPEND SRC_LIST "${CMAKE_CURRENT_LIST_DIR}/mp_${_module}.c")
        else()
            message("cannot find mp_${_module}.c")
        endif()
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

    set(DRM_TYPE "0" CACHE STRING "DRM bo based Allocator  intel, hantro")
    if(${DRM_TYPE} STREQUAL "intel" OR ${DRM_TYPE} STREQUAL "hantro")
        pkg_check_modules (DRM REQUIRED  libdrm libdrm_${DRM_TYPE})
        string(TOUPPER drm_${DRM_TYPE} DRM_DEFINITION)
        add_definitions(-D${DRM_DEFINITION})
        add_definitions(-DDRM_TYPE)
        list(APPEND SRC_LIST ${CMAKE_SOURCE_DIR}/src/drm_allocator/mp_gstdrmbomemory.c)
        list(APPEND SRC_LIST ${CMAKE_SOURCE_DIR}/src/drm_allocator/mp_gstdrmbomemory.h)
    else()
        message("DRM_TYPE is unsupported: ${DRM_TYPE}, skip the build")
    endif()

    option(USE_VPUSMM "Use VPUSMM(VPU Shared memory manager) based allocator" OFF)
    if(USE_VPUSMM)
        pkg_check_modules (VPUSMM REQUIRED libvpusmm)
        list(APPEND SRC_LIST ${CMAKE_SOURCE_DIR}/src/vpusmm_allocator/gstvpusmm.c)
        list(APPEND SRC_LIST ${CMAKE_SOURCE_DIR}/src/vpusmm_allocator/gstvpusmm.h)
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
    set_source_files_properties(
        ${BASE_MODULES_PATH}/mp_crop.c
        ${BASE_MODULES_PATH}/mp_gva_postproc_and_upload.c
        ${BASE_MODULES_PATH}/mp_feeder.c
        ${BASE_MODULES_PATH}/mp_mix.c
        ${BASE_MODULES_PATH}/mp_mix2.c
        ${BASE_MODULES_PATH}/mp_metadata.c
        ${BASE_MODULES_PATH}/mp_openvino.c
        ${BASE_MODULES_PATH}/mp_openvino_tracking.c
        ${BASE_MODULES_PATH}/mp_mix3.c
        ${BASE_MODULES_PATH}/mp_metaparser.c
        ${BASE_MODULES_PATH}/mp_dump_buffer.c
        PROPERTIES LANGUAGE CXX
    )

    add_library(${MEDIAPIPE_NAME} STATIC ${SRC_LIST})

    target_compile_options(${MEDIAPIPE_NAME} PUBLIC $<$<COMPILE_LANGUAGE:C>:-std=gnu99> $<$<COMPILE_LANGUAGE:CXX>:-std=c++11> -Wno-deprecated-declarations -Werror)
    target_compile_definitions(${MEDIAPIPE_NAME} PUBLIC $<$<CONFIG:Debug>:DEBUG>)
    target_compile_definitions(${MEDIAPIPE_NAME} PRIVATE LOAD_ALL_MODULES_BY_DEFAULT)

    if(USE_THREAD_DEFAULT_MAIN_CONTEXT)
        message(STATUS "Use thread default main context")
        target_compile_definitions(${MEDIAPIPE_NAME} PRIVATE USE_THREAD_DEFAULT_MAIN_CONTEXT)
    endif()

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

    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${JSON-C_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GLIB_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GLIB_GOBJECT_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GSTREAMER_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GSTREAMER_BASE_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GSTREAMER_APP_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GSTREAMER_VIDEO_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GSTREAMER_RTSPSERVER_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${CAIRO_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${LIBPANGO_LIBRARIES})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${LIBPANGO_CAIRO_LIBRARY})
    target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${ADDON_MODULES_DEP_LIBS})

    if(${BUILD_APP} STREQUAL "hddl" )
          target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${HDDL_PLUGIN_LIBRARY})
    endif()

    if(DEFINED DRM_DEFINITION)
        target_include_directories(${MEDIAPIPE_NAME} PRIVATE ${DRM_INCLUDE_DIRS})
        target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${DRM_LIBRARIES})
        target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GSTREAMER_ALLOCATORS_LIBRARIES})

        target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/drm_allocator)
    endif()

    if(USE_VPUSMM)
        add_definitions(-DUSE_VPUSMM)
        target_include_directories(${MEDIAPIPE_NAME} PRIVATE ${VPUSMM_INCLUDE_DIRS})
        target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${VPUSMM_LIBRARIES})
        target_link_libraries(${MEDIAPIPE_NAME} PRIVATE ${GSTREAMER_ALLOCATORS_LIBRARIES})

        target_include_directories(${MEDIAPIPE_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/src/vpusmm_allocator)
    endif()

endfunction()
