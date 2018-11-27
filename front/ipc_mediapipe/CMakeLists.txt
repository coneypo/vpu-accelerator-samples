
#defind module_path
set(BASE_MODULES_PATH ${CMAKE_SOURCE_DIR}/src/modules)

#add package find path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/" "${BASE_MODULES_PATH}/cmake/modules")

#define code complile file
FILE(GLOB_RECURSE SRC_LIST "${CMAKE_SOURCE_DIR}/src/config/*.c" "${CMAKE_SOURCE_DIR}/src/core/*.c" "${CMAKE_SOURCE_DIR}/src/utils/*.c" )

#define modules that will be compliled
set(MP_MODULES element rtsp changeformat onvif openvino mix2 encoder)
set(BASE_MODULES ${MP_MODULES})

##add mbqp module
#list(APPEND BASE_MODULES mbqp)
#list(APPEND SRC_LIST  ${BASE_MODULES_PATH}/meta/mp_mbqpmeta.c)
#
##add roi module
#list(APPEND BASE_MODULES roi)
#list(APPEND SRC_LIST  ${BASE_MODULES_PATH}/meta/mp_roimeta.c)
#
##add sei module
#list(APPEND BASE_MODULES sei)
#list(APPEND SRC_LIST  ${BASE_MODULES_PATH}/meta/mp_seimeta.c)

#add mudules source file
foreach(_module ${MP_MODULES})
list(APPEND SRC_LIST "${BASE_MODULES_PATH}/mp_${_module}.c")
endforeach(_module)
list(APPEND SRC_LIST "./main.c")

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
SET_SOURCE_FILES_PROPERTIES( ${BASE_MODULES_PATH}/mp_crop.c ${BASE_MODULES_PATH}/mp_mix.c  ${BASE_MODULES_PATH}/mp_mix2.c PROPERTIES LANGUAGE CXX )
SET_SOURCE_FILES_PROPERTIES( ${BASE_MODULES_PATH}/mp_openvino.c PROPERTIES LANGUAGE CXX )

#copy config.json and launch.txt into make dir
configure_file(./config.json  ${CMAKE_CURRENT_BINARY_DIR}/config.json COPYONLY)
configure_file(./launch.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch.txt COPYONLY)
configure_file(./config_onvif.json  ${CMAKE_CURRENT_BINARY_DIR}/config_onvif.json COPYONLY)
configure_file(./launch_onvif.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_onvif.txt COPYONLY)
configure_file(./multi_mix_cvsdk_config.json  ${CMAKE_CURRENT_BINARY_DIR}/multi_mix_cvsdk_config.json COPYONLY)
configure_file(./multi_mix_cvsdk_launch.txt  ${CMAKE_CURRENT_BINARY_DIR}/multi_mix_cvsdk_launch.txt COPYONLY)
configure_file(./decode_config.json  ${CMAKE_CURRENT_BINARY_DIR}/decode_config.json COPYONLY)
configure_file(./decode_launch.txt  ${CMAKE_CURRENT_BINARY_DIR}/decode_launch.txt COPYONLY)
configure_file(./config_testbuffer.json  ${CMAKE_CURRENT_BINARY_DIR}/config_testbuffer.json COPYONLY)
configure_file(./launch_testbuffer.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_testbuffer.txt COPYONLY)

ADD_EXECUTABLE(mediapipe2 ${SRC_LIST} )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=gnu99 -Wno-deprecated-declarations -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -Wno-deprecated-declarations -Werror")


##find libgstocl.so for compile
#find_package (Gstocl REQUIRED)
#include_directories(${GSTOCL_INCLUDE_DIRS})
#
##inclue meta head file
#include_directories(${BASE_MODULES_PATH}/meta)

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
include_directories("${CMAKE_SOURCE_DIR}/src/config")
include_directories("${CMAKE_SOURCE_DIR}/src/core")
include_directories("${CMAKE_SOURCE_DIR}/src/meta")
include_directories("${CMAKE_SOURCE_DIR}/src/utils")

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

#set FOLDER for subdir install
set_property(TARGET mediapipe2 PROPERTY FOLDER "executables")

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
install(FILES ./config.json DESTINATION /etc/mediapipe2)
install(FILES ./launch.txt DESTINATION /etc/mediapipe2)
install(FILES ./config_onvif.json DESTINATION /etc/mediapipe2)
install(FILES ./launch_onvif.txt DESTINATION /etc/mediapipe2)

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

