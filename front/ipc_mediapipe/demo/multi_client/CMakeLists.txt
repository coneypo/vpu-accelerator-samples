
#defind module_path
set(BASE_MODULES_PATH ${CMAKE_SOURCE_DIR}/src/modules)

#add package find path
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/modules/" "${BASE_MODULES_PATH}/cmake/modules")

#define code complile file
FILE(GLOB_RECURSE SRC_LIST "${CMAKE_SOURCE_DIR}/src/config/*.c" "${CMAKE_SOURCE_DIR}/src/core/*.c" "${CMAKE_SOURCE_DIR}/src/utils/*.c" )

#define modules that will be compliled
set(MP_MODULES teststream crop_va)
set(BASE_MODULES ${MP_MODULES})

#add mudules source file
foreach(_module ${MP_MODULES})
list(APPEND SRC_LIST "${BASE_MODULES_PATH}/mp_${_module}.c")
endforeach(_module)
list(APPEND SRC_LIST "./main.c")


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
SET_SOURCE_FILES_PROPERTIES( ${BASE_MODULES_PATH}/mp_crop_va.c PROPERTIES LANGUAGE CXX )

set(TARGETS_NAME multi_client)

ADD_EXECUTABLE(${TARGETS_NAME} ${SRC_LIST} )

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -g -std=gnu99 -Wno-deprecated-declarations -Werror")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -Wno-deprecated-declarations -Werror")

find_package (Json-c REQUIRED)
find_package (Cairo REQUIRED)
find_package (Glib REQUIRED)
find_package (Gstreamer REQUIRED)
find_package (Pango REQUIRED)

find_package(OpenCV REQUIRED core imgproc)
include_directories(${OpenCV_INCLUDE_DIRS})

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
set_target_properties(${TARGETS_NAME} PROPERTIES LINKER_LANGUAGE CXX)
target_link_libraries (${TARGETS_NAME} ${OpenCV_LIBS})

target_link_libraries (${TARGETS_NAME} ${JSON-C_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${GLIB_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${GLIB_GOBJECT_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${GSTREAMER_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${GSTREAMER_BASE_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${GSTREAMER_APP_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${GSTREAMER_VIDEO_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${GSTREAMER_RTSPSERVER_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${CAIRO_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${LIBPANGO_LIBRARIES})
target_link_libraries (${TARGETS_NAME} ${LIBPANGO_CAIRO_LIBRARY})
target_link_libraries (${TARGETS_NAME} ${ADDON_MODULES_DEP_LIBS})

#set FOLDER for subdir install
set_property(TARGET ${TARGETS_NAME} PROPERTY FOLDER "executables")

configure_file(./launch_test_stream.txt  ${CMAKE_CURRENT_BINARY_DIR}/launch_test_stream.txt COPYONLY)
configure_file(./config_test_stream.json  ${CMAKE_CURRENT_BINARY_DIR}/config_test_stream.json COPYONLY)

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

if(CMAKE_BUILD_TYPE STREQUAL Debug)
    add_definitions(-DDEBUG)
endif()

