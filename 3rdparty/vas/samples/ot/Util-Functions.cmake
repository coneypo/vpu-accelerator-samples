#
# It will define global list what adds items.
# And, it removes duplicates.
#
# EX) func_global_set(out_list item1 item2 item3...)
# --- Made by func_global_set will not be updated.
#
function (func_global_set out_list)
    list (REMOVE_DUPLICATES ARGN)
    set (${out_list} ${ARGN} CACHE INTERNAL "")
endfunction ()

#
# EX) func_find_library (MKL_LIBRARIES ${mkl_dir_path} mkl_core mkl_sequential)
#
function (func_find_library TARGET DIRECTORY)
    unset (${TARGET} CACHE)

    foreach (var ${ARGN})
        find_library (${var}_out
                NAMES ${var}
                PATHS ${DIRECTORY} NO_DEFAULT_PATH)

        if (NOT ${${var}_out} STREQUAL "${var}_out-NOTFOUND")
            list (APPEND ${TARGET}_temp ${${var}_out})
        else ()
            message (SEND_ERROR "\"${var}\" couldn't be found in ${DIRECTORY}")
        endif ()


        unset (${var}_out CACHE)
    endforeach ()

    func_global_set (${TARGET} ${${TARGET}_temp})
endfunction ()

#
# don't use this function commonly.
# This func should be called for Visual Studio 2015(v14) to insert Debugging Environment.
#
function (func_generate_and_set_visual_studio_props_file IN_DIRECTORY IN_PROJECT_NAME)
    if (WIN32)
        file (WRITE "${IN_DIRECTORY}/${IN_PROJECT_NAME}.user.props" [=[
<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" ToolsVersion="14.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <LocalDebuggerEnvironment>PATH=$(INTEL_CVSDK_DIR)\opencv\bin;$(INTEL_CVSDK_DIR)\deployment_tools\inference_engine\bin\intel64\Debug;$(PATH_VAS)\bin\intel64;%PATH%</LocalDebuggerEnvironment>
  </PropertyGroup>
  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <LocalDebuggerEnvironment>PATH=$(INTEL_CVSDK_DIR)\opencv\bin;$(INTEL_CVSDK_DIR)\deployment_tools\inference_engine\bin\intel64\Release;$(PATH_VAS)\bin\intel64;%PATH%</LocalDebuggerEnvironment>
  </PropertyGroup>
</Project>
]=])

        set_target_properties (${IN_PROJECT_NAME}
            PROPERTIES
            VS_USER_PROPS "${IN_PROJECT_NAME}.user.props")
    endif ()
endfunction ()
