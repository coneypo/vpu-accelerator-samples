# find_package(InferenceEngine REQUIRED HINTS "/opt/intel/dldt/inference-engine/build")

find_package(LibVA REQUIRED)

find_package(Gstreamer REQUIRED)
find_package(Glib REQUIRED)

find_path(BOOST_INC_DIR NAMES boost/property_tree/json_parser.hpp
	HINTS $ENV{INSTALL_DIR}/include)
if(NOT BOOST_INC_DIR)
	message(FATAL_ERROR "BOOST header not found. hints path: $ENV{INSTALL_DIR}/include/boost")
endif()

message("boost header: ${BOOST_INC_DIR}")

set(SAMPLE_INC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/KmbPipelineDecOnly ${CMAKE_CURRENT_SOURCE_DIR}/../../inc/util)

file(GLOB_RECURSE SAMPLE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp ${CMAKE_CURRENT_SOURCE_DIR}/../util/*.cpp)

add_executable(GstHvaSampleDecOnly "${SAMPLE_SRC}")
target_include_directories(GstHvaSampleDecOnly PUBLIC "$<BUILD_INTERFACE:${SAMPLE_INC_DIR}>")

target_include_directories(GstHvaSampleDecOnly PUBLIC "${GSTREAMER_INCLUDE_DIRS}" "${GLIB_INCLUDE_DIRS}" "${BOOST_INC_DIR}")

target_include_directories(GstHvaSampleDecOnly PUBLIC "${LIBVA_INCLUDE_DIRS}")

##target_link_libraries(GstHvaSample glib-2.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 gstapp-1.0 gstallocators-1.0 vpusmm)
target_link_libraries(GstHvaSampleDecOnly glib-2.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 gstapp-1.0 gstallocators-1.0)

target_link_libraries(GstHvaSampleDecOnly dl pthread boost_filesystem)

target_link_libraries(GstHvaSampleDecOnly hva)

target_link_libraries(GstHvaSampleDecOnly "${LIBVA_LIBRARIES}")

get_target_property(SAMPLE_INC_DIR GstHvaSampleDecOnly INCLUDE_DIRECTORIES)
message("SAMPLE_INC_DIR: ${SAMPLE_INC_DIR}")

get_target_property(NEW_SAMPLE_LINK_FLAGS GstHvaSampleDecOnly LINK_FLAGS)
message("NEW_SAMPLE_LINK_FLAGS: ${NEW_SAMPLE_LINK_FLAGS}")
