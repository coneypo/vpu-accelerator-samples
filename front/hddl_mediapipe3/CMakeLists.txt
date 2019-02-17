#defind module_path
set(BASE_MODULES_PATH ${CMAKE_SOURCE_DIR}/src/modules)

#add package find path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/" "${BASE_MODULES_PATH}/cmake/modules")

#define code complile file
file(GLOB_RECURSE SRC_LIST "${CMAKE_SOURCE_DIR}/src/config/*.c" "${CMAKE_SOURCE_DIR}/src/core/*.c" "${CMAKE_SOURCE_DIR}/src/utils/*.c" )

#define modules that will be compliled
set(MP_MODULES element)
set(BASE_MODULES ${MP_MODULES})

#add mudules source file
foreach(_module ${MP_MODULES})
list(APPEND SRC_LIST "${BASE_MODULES_PATH}/mp_${_module}.c")
endforeach(_module)
file(GLOB_RECURSE LOCAL_HDRS "*.h")
list(APPEND SRC_LIST main.c hddl_mediapipe.c us_client.c message.cpp ${LOCAL_HDRS})

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

find_package(Protobuf REQUIRED)
protobuf_generate_cpp(PROTO_SRC PROTO_HDR ../message/hddl_message.proto)

# set some module compiled by CXX
set_source_files_properties(${BASE_MODULES_PATH}/mp_crop.c ${BASE_MODULES_PATH}/mp_mix.c ${BASE_MODULES_PATH}/mp_mix2.c ${BASE_MODULES_PATH}/mp_metadata.c PROPERTIES LANGUAGE CXX)
set_source_files_properties(${BASE_MODULES_PATH}/mp_openvino.c PROPERTIES LANGUAGE CXX)

add_executable(hddl_mediapipe3 ${SRC_LIST} ${PROTO_SRC} ${PROTO_HDR})

target_compile_options(hddl_mediapipe3 PRIVATE $<$<COMPILE_LANGUAGE:C>:-std=gnu99> $<$<COMPILE_LANGUAGE:CXX>:-std=c++11> -Wno-deprecated-declarations -Werror)

find_package(Json-c REQUIRED)
find_package(Cairo REQUIRED)
find_package(Glib REQUIRED)
find_package(Gstreamer REQUIRED)
find_package(Pango REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)

include_directories(${JSON-C_INCLUDE_DIRS})
include_directories(${CAIRO_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBPANGO_INCLUDE_DIRS})
include_directories("${CMAKE_SOURCE_DIR}/src/config")
include_directories("${CMAKE_SOURCE_DIR}/src/core")
include_directories("${CMAKE_SOURCE_DIR}/src/meta")
include_directories("${CMAKE_SOURCE_DIR}/src/utils")
include_directories("${CMAKE_SOURCE_DIR}/front/hddl_mediapipe3")
include_directories("${PROTOBUF_INCLUDE_DIR}")
include_directories("${CMAKE_CURRENT_BINARY_DIR}")

#opencl oclcommon.h didn't not add extern c heard, so there is some problem
#when link libgstocl.so, so force use CXX link
set_target_properties(hddl_mediapipe3 PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries(hddl_mediapipe3 ${JSON-C_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${GLIB_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${GLIB_GOBJECT_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${GSTREAMER_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${GSTREAMER_BASE_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${GSTREAMER_APP_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${GSTREAMER_VIDEO_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${GSTREAMER_RTSPSERVER_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${CAIRO_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${LIBPANGO_LIBRARIES})
target_link_libraries(hddl_mediapipe3 ${LIBPANGO_CAIRO_LIBRARY})
target_link_libraries(hddl_mediapipe3 ${ADDON_MODULES_DEP_LIBS})
target_link_libraries(hddl_mediapipe3 pthread)
target_link_libraries(hddl_mediapipe3 ${PROTOBUF_LITE_LIBRARY})
target_link_libraries(hddl_mediapipe3 ${Boost_LIBRARIES})

#set FOLDER for subdir install
set_property(TARGET hddl_mediapipe3 PROPERTY FOLDER "executables")

message("base moudle ${BASE_MODULES}")
message("base deps lib")
message("${JSON-C_LIBRARIES}")
message("${GLIB_LIBRARIES}")
message("${GLIB_GOBJECT_LIBRARIES}")
message("${GSTREAMER_LIBRARIES}")
message("${GSTREAMER_BASE_LIBRARIES}")
message("${GSTREAMER_APP_LIBRARIES}")
message("${GSTREAMER_VIDEO_LIBRARIES}")
message("${GSTREAMER_RTSPSERVER_LIBRARIES}")
message("${CAIRO_LIBRARIES}")
message("${LIBPANGO_LIBRARIES}")
message("${LIBPANGO_CAIRO_LIBRARY}")
message("addon moudle ${ADDON_MODULES}")
message("addon deps lib")
message("${ADDON_MODULES_DEP_LIBS}")

install(TARGETS hddl_mediapipe3
        RUNTIME DESTINATION bin
        ARCHIVE DESTINATION lib
        LIBRARY DESTINATION lib)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

