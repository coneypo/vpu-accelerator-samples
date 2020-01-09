cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(GstHvaSample)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
#find_package(Gstreamer REQUIRED)
#find_package(Glib REQUIRED)

#find_path(BOOST_INC_DIR NAMES boost/property_tree/json_parser.hpp
#	HINTS $ENV{INSTALL_DIR}/include)
#if(NOT BOOST_INC_DIR)
#	message(FATAL_ERROR "BOOST header not found. hints path: $ENV{INSTALL_DIR}/include/boost")
#endif()

#message(INFO "boost header: ${BOOST_INC_DIR}")

include_directories( "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/hva/"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/hva/inc/api"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/hva/inc/scheduler"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/hva/inc/util"
  "${CMAKE_CURRENT_SOURCE_DIR}/3rdparty/hva/3rdparty/ade/include")

add_library(hva SHARED IMPORTED)
set_target_properties(hva PROPERTIES
    IMPORTED_LOCATION "${CMAKE_CURRENT_SOURCE_DIR}/lib/libhva.so"
)

add_subdirectory(src)
