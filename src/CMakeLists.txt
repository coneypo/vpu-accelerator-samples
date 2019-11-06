find_package(InferenceEngine REQUIRED HINTS "/opt/intel/dldt/inference-engine/build")

set(SAMPLE_INC_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../inc")

file(GLOB SAMPLE_SRC ${CMAKE_CURRENT_SOURCE_DIR}/*.cpp)

add_executable(GstHvaSample "${SAMPLE_SRC}")
target_include_directories(GstHvaSample PUBLIC "$<BUILD_INTERFACE:${SAMPLE_INC_DIR}>")

target_include_directories(GstHvaSample PUBLIC "${GSTREAMER_INCLUDE_DIRS}" "${GLIB_INCLUDE_DIRS}")
target_link_libraries(GstHvaSample glib-2.0 gstreamer-1.0 gstbase-1.0 gobject-2.0 gstapp-1.0 gstallocators-1.0 vpusmm)

target_link_libraries(GstHvaSample "${InferenceEngine_LIBRARIES}" dl)
target_include_directories(GstHvaSample PUBLIC "${InferenceEngine_INCLUDE_DIRS}")

target_link_libraries(GstHvaSample hva)

#add opencv dependency
find_package(OpenCV REQUIRED)
target_link_libraries(GstHvaSample "${OpenCV_LIBRARIES}" dl)
# message("GstHvaSample linked libraries: ${GLIB_LIBRARIES} ${GSTREAMER_LIBRARIES} ${GSTREAMER_BASE_LIBRARIES}")

# get_target_property(SAMPLE_LINK_FLAGS GstHvaSample LINK_FLAGS)
# set_target_properties(GstHvaSample PROPERTIES LINK_FLAGS "${SAMPLE_LINK_FLAGS} -L${GLIB_LIBRARY_DIRS} -L${GSTREAMER_LIBRARY_DIR} -L${GSTREAMER_BASE_LIBRARY_DIR}")
# target_link_directories(GstHvaSample PUBLIC GLIB_LIBRARy_DIRS GSTREAMER_LIBRARY_DIR GSTREAMER_BASE_LIBRARY_DIR)

get_target_property(SAMPLE_INC_DIR GstHvaSample INCLUDE_DIRECTORIES)
message("SAMPLE_INC_DIR: ${SAMPLE_INC_DIR}")

get_target_property(NEW_SAMPLE_LINK_FLAGS GstHvaSample LINK_FLAGS)
message("NEW_SAMPLE_LINK_FLAGS: ${NEW_SAMPLE_LINK_FLAGS}")