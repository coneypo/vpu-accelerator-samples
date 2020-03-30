file(GLOB HDRS "*.h")
file(GLOB SRCS "*.cpp")

set(TARGET_NAME fake_result_sender)
add_executable(${TARGET_NAME} ${SRCS} ${HDRS})
target_compile_options(${TARGET_NAME} PRIVATE -Wall -Wformat -Wformat-security -Werror=format-security -fstack-protector-strong -fno-strict-overflow -fno-delete-null-pointer-checks -fPIE -fwrapv -z noexecstack -z,now -z,relro -D_FORTIFY_SOURCE=2)
target_link_libraries(${TARGET_NAME} PRIVATE hddldemoutils)
target_link_libraries(${TARGET_NAME} PRIVATE ${Boost_LIBRARIES})

install(TARGETS ${TARGET_NAME} DESTINATION bin)