file(GLOB HDRS "*.h")
file(GLOB SRCS "*.cpp")

set(TARGET_NAME fake_result_sender)
add_executable(${TARGET_NAME} ${SRCS} ${HDRS})

target_link_libraries(${TARGET_NAME} PRIVATE utils_static)
target_link_libraries(${TARGET_NAME} PRIVATE ${Boost_LIBRARIES})
