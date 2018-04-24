#current file dir is ${CMAKE_CURRENT_LIST_DIR}
#it will be include by mediapipe2

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_LIST_DIR}/cmake/modules/")

#find libgstocl.so for compile
find_package (Gstocl REQUIRED)
include_directories(${GSTOCL_INCLUDE_DIRS})

#inclue meta head file
include_directories(${CMAKE_CURRENT_LIST_DIR}/meta)

#define  modules
set(ADDON_MODULES mix cvsdk encoder idr longterm changeformat channel slice crop snapshot onvif)

#add mbqp module
list(APPEND ADDON_MODULES mbqp)
list(APPEND SRC_LIST  ${CMAKE_CURRENT_LIST_DIR}/meta/mp_mbqpmeta.c)

#add roi module
list(APPEND ADDON_MODULES roi)
list(APPEND SRC_LIST  ${CMAKE_CURRENT_LIST_DIR}/meta/mp_roimeta.c)

#add sei module
list(APPEND ADDON_MODULES sei)
list(APPEND SRC_LIST  ${CMAKE_CURRENT_LIST_DIR}/meta/mp_seimeta.c)

#add modules source file
FOREACH(_module ${ADDON_MODULES})
list(APPEND SRC_LIST "${CMAKE_CURRENT_LIST_DIR}/mp_${_module}.c")
ENDFOREACH(_module)


#opencl oclcommon.h didn't not add  extern c heard, so there is some problem
#let mp_crop.c mp_mix.c compiled by CXX
SET_SOURCE_FILES_PROPERTIES( ${CMAKE_CURRENT_LIST_DIR}/mp_crop.c ${CMAKE_CURRENT_LIST_DIR}/mp_mix.c  PROPERTIES LANGUAGE CXX )

list(APPEND MP_MODULES ${ADDON_MODULES})
list(APPEND ADDON_MODULES_DEP_LIBS  ${GSTOCL_LIBRARIES})
