# cmake v3.8.2: added VS_USER_PROPS. It will add some path to run samples on VisualStudio.
# https://cmake.org/cmake/help/v3.8/prop_tgt/VS_USER_PROPS.html
cmake_minimum_required (VERSION 3.8.2)
cmake_policy (SET CMP0054 NEW)

project (ObjectTrackerSample)

include (${CMAKE_HOME_DIRECTORY}/Util-Functions.cmake)

set (INTEL_CVSDK_DIR $ENV{INTEL_CVSDK_DIR} CACHE PATH "OpenVINO")
set (PATH_VAS $ENV{PATH_VAS} CACHE PATH "Video Analytics Suites")

include (${INTEL_CVSDK_DIR}/opencv/cmake/OpenCVConfig.cmake)
add_subdirectory (${INTEL_CVSDK_DIR}/deployment_tools/inference_engine/samples/cpp/thirdparty/gflags build)


#
# VAS
#
add_library (common_vas INTERFACE)

set_target_properties (common_vas PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${PATH_VAS}/include;${OpenCV_INCLUDE_DIRS}")

if (WIN32)
    func_find_library (VAS_LIBRARIES_DEBUG "${PATH_VAS}/lib/intel64/Debug" vaspvd_d vasot_d)
    func_find_library (VAS_LIBRARIES_RELEASE "${PATH_VAS}/lib/intel64/Release" vaspvd vasot)

    target_link_libraries (common_vas INTERFACE
        debug "${VAS_LIBRARIES_DEBUG}"
        optimized "${VAS_LIBRARIES_RELEASE}")
else ()
    func_find_library (VAS_LIBRARIES "${PATH_VAS}/lib/intel64" vaspvd vasot)

    target_link_libraries (common_vas INTERFACE
        "${VAS_LIBRARIES}"
        dl pthread)
endif ()

target_link_libraries (common_vas INTERFACE ${OpenCV_LIBRARIES} gflags)


add_definitions (-DPATH_RES="${CMAKE_HOME_DIRECTORY}/res")

set (CMAKE_POSITION_INDEPENDENT_CODE ON)    # add_definitions(-fPIC)
set (CMAKE_CXX_STANDARD 11)                 # -std=c++11: Supported values are 98, 11 and 14.
set (CMAKE_CXX_STANDARD_REQUIRED ON)
set (CMAKE_CXX_EXTENSIONS OFF)
set (CMAKE_CONFIGURATION_TYPES "Debug;Release" CACHE STRING "" FORCE)


#########################################################################
# Long-term type Object Tracker.
#

set (LONG_TERM_SAMPLE "longterm_type_tracker")

add_executable (${LONG_TERM_SAMPLE} "")

target_sources (${LONG_TERM_SAMPLE} PRIVATE
    src/object_tracker_longterm_sample.cpp)

target_link_libraries (${LONG_TERM_SAMPLE} common_vas)

if (WIN32)
    func_generate_and_set_visual_studio_props_file (${CMAKE_CURRENT_BINARY_DIR} ${LONG_TERM_SAMPLE})
endif ()


#########################################################################
# Short-term type Object Tracker.
#

set (SHORT_TERM_SAMPLE "shortterm_type_tracker")

add_executable (${SHORT_TERM_SAMPLE} "")

target_sources (${SHORT_TERM_SAMPLE} PRIVATE
    src/object_tracker_shortterm_sample.cpp)

target_link_libraries (${SHORT_TERM_SAMPLE} common_vas)

if (WIN32)
    func_generate_and_set_visual_studio_props_file (${CMAKE_CURRENT_BINARY_DIR} ${SHORT_TERM_SAMPLE})
endif ()


#########################################################################
# Multi-threaded short-term type Object Tracker.
#

set (SHORT_TERM_MT_SAMPLE "shortterm_type_tracker_mt")

add_executable (${SHORT_TERM_MT_SAMPLE} "")

target_sources (${SHORT_TERM_MT_SAMPLE} PRIVATE
    src/object_tracker_shortterm_mt_sample.cpp)

target_link_libraries (${SHORT_TERM_MT_SAMPLE} common_vas)

if (WIN32)
    func_generate_and_set_visual_studio_props_file (${CMAKE_CURRENT_BINARY_DIR} ${SHORT_TERM_MT_SAMPLE})
endif ()


#########################################################################
# Demo sample for object counting
#

set (SHORT_TERM_SAMPLE_DEMO "shortterm_object_counting")

add_executable (${SHORT_TERM_SAMPLE_DEMO} "")

target_sources (${SHORT_TERM_SAMPLE_DEMO} PRIVATE
    src/object_tracker_shortterm_object_count_sample.cpp)

target_link_libraries (${SHORT_TERM_SAMPLE_DEMO} common_vas)

add_definitions (-DPATH_OUTPUT="${CMAKE_HOME_DIRECTORY}")

if (WIN32)
    func_generate_and_set_visual_studio_props_file (${CMAKE_CURRENT_BINARY_DIR} ${SHORT_TERM_SAMPLE_DEMO})
endif ()


#########################################################################
# Zero-term type Object Tracker.
#

set (ZERO_TERM_SAMPLE "zeroterm_type_tracker")

add_executable (${ZERO_TERM_SAMPLE} "")

target_sources (${ZERO_TERM_SAMPLE} PRIVATE
    src/object_tracker_zeroterm_sample.cpp)

target_link_libraries (${ZERO_TERM_SAMPLE} common_vas)

if (WIN32)
    func_generate_and_set_visual_studio_props_file (${CMAKE_CURRENT_BINARY_DIR} ${ZERO_TERM_SAMPLE})
endif ()
