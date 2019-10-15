# Copyright (C) 2019 Intel Corporation
# SPDX-License-Identifier: MIT

mediapipe_library_build(NAME xlinkreaderlib MODULES element xlinkreader)

set(TargetName xlinkreader)
set_source_files_properties(mp_xlinkreader.c PROPERTIES LANGUAGE CXX)

add_executable(${TargetName} main.c mp_xlinkreader.c)
target_link_libraries(${TargetName} PRIVATE xlinkreaderlib XLinkARM pthread)
set_target_properties(${TargetName} PROPERTIES FOLDER "executables" LINKER_LANGUAGE CXX)
target_compile_definitions(${TargetName} PRIVATE $<$<CONFIG:Debug>:DEBUG>)
