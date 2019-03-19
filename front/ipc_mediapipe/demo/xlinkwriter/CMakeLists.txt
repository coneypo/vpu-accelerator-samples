mediapipe_library_build(NAME xlinkwriterlib MODULES element xlinkwriter)

set(TargetName xlinkwriter)
set_source_files_properties(mp_xlinkwriter.c PROPERTIES LANGUAGE CXX)

add_executable(${TargetName} main.c mp_xlinkwriter.c)
target_link_libraries(${TargetName} PRIVATE xlinkwriterlib XLinkPC pthread)
set_target_properties(${TargetName} PROPERTIES FOLDER "executables" LINKER_LANGUAGE CXX)
target_compile_definitions(${TargetName} PRIVATE $<$<CONFIG:Debug>:DEBUG>)
