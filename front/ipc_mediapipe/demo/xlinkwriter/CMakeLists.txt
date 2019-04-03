mediapipe_library_build(NAME xlinkwriterlib MODULES element xlinkwriter)

set(TargetName xlinkwriter)
set_source_files_properties(mp_xlinkwriter.c PROPERTIES LANGUAGE CXX)

add_executable(${TargetName} h264_stream_source_dummy.c mp_xlinkwriter.c)
target_link_libraries(${TargetName} PRIVATE xlinkwriterlib XLinkPC pthread)
set_target_properties(${TargetName} PROPERTIES FOLDER "executables" LINKER_LANGUAGE CXX)
target_compile_definitions(${TargetName} PRIVATE $<$<CONFIG:Debug>:DEBUG>)

add_executable(testh265stream h265_stream_source.c)
target_link_libraries(testh265stream PRIVATE xlinkwriterlib XLinkPC pthread)
target_compile_definitions(testh265stream PRIVATE $<$<CONFIG:Debug>:DEBUG>)
