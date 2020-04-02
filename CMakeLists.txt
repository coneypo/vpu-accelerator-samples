cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(GstHvaSample)

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

set(HVA_SAMPLE_COMPILER_FLAGS "-Wformat -Wformat-security -Werror=format-security -D_FORTIFY_SOURCE=2 -fstack-protector-strong -fno-strict-overflow -fwrapv -fno-delete-null-pointer-checks -fPIE -fPIC")
set(HVA_SAMPLE_SHARED_LINKER_FLAGS "-z relro -z now -z noexecstack")
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} ${HVA_SAMPLE_COMPILER_FLAGS}")
set(CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} ${HVA_SAMPLE_COMPILER_FLAGS}")
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${HVA_SAMPLE_SHARED_LINKER_FLAGS}")
set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} ${HVA_SAMPLE_SHARED_LINKER_FLAGS}")

# set(CMAKE_VERBOSE_MAKEFILE ON)

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
