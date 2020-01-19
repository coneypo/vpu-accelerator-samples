find_package(Boost REQUIRED system filesystem)
message(STATUS "boost library: " ${Boost_LIBRARIES})

add_subdirectory(fake_result_sender)
add_subdirectory(fake_result_receiver)
add_subdirectory(rob_test)
