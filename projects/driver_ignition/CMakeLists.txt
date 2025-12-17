# Driver CMake project
cmake_minimum_required(VERSION 3.16)

project(driver_ignition)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/externals/nlohmann)
include_directories(${CMAKE_SOURCE_DIR}/externals/openvr)
include_directories(${CMAKE_SOURCE_DIR}/projects/common)

add_library(driver_ignition SHARED
  driver.cpp
)

target_link_libraries(driver_ignition PRIVATE ignition_common ipc_ignition_static)
set_target_properties(driver_ignition PROPERTIES LINKER_LANGUAGE CXX)

if (CMAKE_HOST_WIN32)
  target_link_libraries(driver_ignition PRIVATE shlwapi)
endif()