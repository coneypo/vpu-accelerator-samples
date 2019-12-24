#
#Copyright (C) 2019 Intel Corporation
#
#SPDX-License-Identifier: MIT
#
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

function(SET_TARGET_OUTPUT_DIRS target dir_archive dir_library dir_runtime)
    if(${CMAKE_VERSION} VERSION_GREATER "3.0.1")
        set_target_properties(
            ${target}
            PROPERTIES
            ARCHIVE_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/output/${dir_archive}"
            ARCHIVE_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/output/${dir_archive}"
            ARCHIVE_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/output/${dir_archive}"
            ARCHIVE_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/output/${dir_archive}"
            ARCHIVE_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/output/${dir_archive}"
            LIBRARY_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/output/${dir_library}"
            LIBRARY_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/output/${dir_library}"
            LIBRARY_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/output/${dir_library}"
            LIBRARY_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/output/${dir_library}"
            LIBRARY_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/output/${dir_library}"
            RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}/output/${dir_runtime}"
            RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}/output/${dir_runtime}"
            RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}/output/${dir_runtime}"
            RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}/output/${dir_runtime}"
            RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}/output/${dir_runtime}"
        )
    endif()
endfunction()

function(ADD_COPY_DIRECTORY copy_target src_dir dest_dir)
    get_filename_component(src_dir "${src_dir}" ABSOLUTE)
    if(NOT TARGET ${copy_target})
        add_custom_target(${copy_target} ALL)
    endif()
    get_filename_component(dest_dir "${dest_dir}" ABSOLUTE)
    file(GLOB_RECURSE src_files RELATIVE "${src_dir}" "${src_dir}/*")
    foreach(file ${src_files})
        set(input "${src_dir}/${file}")
        set(output "${dest_dir}/${file}")
        get_filename_component(input_abs "${input}" ABSOLUTE)
        get_filename_component(output_abs "${output}" ABSOLUTE)
        get_filename_component(directory "${output}" DIRECTORY)
        if(NOT EXISTS ${directory})
            file(MAKE_DIRECTORY ${directory})
            message(STATUS "Making directory ${directory}")
        endif()
        if(NOT "${input_abs}" STREQUAL "${output_abs}")
            add_custom_command(
                TARGET ${copy_target}
                PRE_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${input_abs} ${output_abs}
                COMMENT "Copying ${file} to ${dest_dir}"
                DEPENDS "${input}"
            )
        endif()
    endforeach()
endfunction()

function(SET_NUGET_TARGET_WITHNATIVE id version targets)
    set(${targets} ${CMAKE_BINARY_DIR}/packages/${id}.${version}/build/native/${id}.targets PARENT_SCOPE)
endfunction()

function(SET_NUGET_TARGET id version targets)
    set(${targets} ${CMAKE_BINARY_DIR}/packages/${id}.${version}/build/${id}.targets PARENT_SCOPE)
endfunction()
