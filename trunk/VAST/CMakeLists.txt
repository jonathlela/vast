cmake_minimum_required(VERSION 2.6)
project(VAST)

set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/lib")
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin")

set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/buildtools/cmake/")
find_package(ACE)

add_subdirectory(common)
add_subdirectory(VASTnet)
add_subdirectory(VAST)
add_subdirectory(VASTsim)
add_subdirectory(Demo)



