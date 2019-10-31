cmake_minimum_required(VERSION 3.5)
set(CMAKE_CXX_STANDARD 11)
project(GstHvaSample)

set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})
find_package(Gstreamer REQUIRED)
find_package(Glib REQUIRED)

add_subdirectory(src)