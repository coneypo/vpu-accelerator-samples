PROJECT(mediapipe2)
cmake_minimum_required(VERSION 2.8)

set(ADDON_MODULE_PATH "" CACHE STRING "addon modules path")
set(ENABLE_EXAMPLE_MODULE "YES" CACHE STRING "enable example modules")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/")
FILE(GLOB_RECURSE SRC_LIST "./src/config/*.c" "./src/core/*.c" "./src/utils/*.c" )

set(MP_MODULES element rtsp )
set(BASE_MODULES ${MP_MODULES})


foreach(_module ${MP_MODULES})
list(APPEND SRC_LIST "./src/modules/mp_${_module}.c")
endforeach(_module)
list(APPEND SRC_LIST "./test/main.c")

#add example moudle
if (${ENABLE_EXAMPLE_MODULE} STREQUAL "YES")
    if(EXISTS ${CMAKE_CURRENT_LIST_DIR}/src/modules/example_module/CMakeLists.txt)
        include(${CMAKE_CURRENT_LIST_DIR}/src/modules/example_module/CMakeLists.txt)
    endif(EXISTS ${CMAKE_CURRENT_LIST_DIR}/src/modules/example_module/CMakeLists.txt)
endif()

#add addon moudle
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


#create  ${modulecfile}
set(modulecfile ${CMAKE_BINARY_DIR}/mp_modules.c)
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

#copy config.json and launch.txt into make dir
configure_file(./test/config.json  ${CMAKE_BINARY_DIR}/config.json COPYONLY)
configure_file(./test/launch.txt  ${CMAKE_BINARY_DIR}/launch.txt COPYONLY)
configure_file(./test/config_onvif.json  ${CMAKE_BINARY_DIR}/config_onvif.json COPYONLY)
configure_file(./test/launch_onvif.txt  ${CMAKE_BINARY_DIR}/launch_onvif.txt COPYONLY)
configure_file(./test/multi_mix_cvsdk_config.json  ${CMAKE_BINARY_DIR}/multi_mix_cvsdk_config.json COPYONLY)
configure_file(./test/multi_mix_cvsdk_launch.txt  ${CMAKE_BINARY_DIR}/multi_mix_cvsdk_launch.txt COPYONLY)
configure_file(./test/decode_config.json  ${CMAKE_BINARY_DIR}/decode_config.json COPYONLY)
configure_file(./test/decode_launch.txt  ${CMAKE_BINARY_DIR}/decode_launch.txt COPYONLY)

ADD_EXECUTABLE(mediapipe2 ${SRC_LIST} )
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=gnu99 -Wno-deprecated-declarations -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -Wno-deprecated-declarations -Werror")


find_package (Json-c REQUIRED)
find_package (Cairo REQUIRED)
find_package (Glib REQUIRED)
find_package (Gstreamer REQUIRED)
find_package (Pango REQUIRED)

include_directories(${JSON-C_INCLUDE_DIRS})
include_directories(${CAIRO_INCLUDE_DIRS})
include_directories(${GLIB_INCLUDE_DIRS})
include_directories(${GSTREAMER_INCLUDE_DIRS})
include_directories(${LIBPANGO_INCLUDE_DIRS})
include_directories("./src/config")
include_directories("./src/core")
include_directories("./src/meta")
include_directories("./src/utils")

#opencl oclcommon.h didn't not add  extern c heard, so there is some problem
#when link  libgstocl.so , so force use  CXX link
set_target_properties(mediapipe2 PROPERTIES LINKER_LANGUAGE CXX)

target_link_libraries (mediapipe2 ${JSON-C_LIBRARIES})
target_link_libraries (mediapipe2 ${GLIB_LIBRARIES})
target_link_libraries (mediapipe2 ${GLIB_GOBJECT_LIBRARIES})
target_link_libraries (mediapipe2 ${GSTREAMER_LIBRARIES})
target_link_libraries (mediapipe2 ${GSTREAMER_BASE_LIBRARIES})
target_link_libraries (mediapipe2 ${GSTREAMER_APP_LIBRARIES})
target_link_libraries (mediapipe2 ${GSTREAMER_VIDEO_LIBRARIES})
target_link_libraries (mediapipe2 ${GSTREAMER_RTSPSERVER_LIBRARIES})
target_link_libraries (mediapipe2 ${CAIRO_LIBRARIES})
target_link_libraries (mediapipe2 ${LIBPANGO_LIBRARIES})
target_link_libraries (mediapipe2 ${LIBPANGO_CAIRO_LIBRARY})
target_link_libraries (mediapipe2 ${ADDON_MODULES_DEP_LIBS})


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

install(TARGETS mediapipe2 DESTINATION /usr/bin)
install(FILES ./test/config.json DESTINATION /etc/mediapipe2)
install(FILES ./test/launch.txt DESTINATION /etc/mediapipe2)
install(FILES ./test/config_onvif.json DESTINATION /etc/mediapipe2)
install(FILES ./test/launch_onvif.txt DESTINATION /etc/mediapipe2)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

