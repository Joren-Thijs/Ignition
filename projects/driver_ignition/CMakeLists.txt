cmake_minimum_required(VERSION 3.16)

project(driver_ignition)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${IGNITION_ROOT}/externals/nlohmann)
include_directories(${IGNITION_ROOT}/externals/openvr)
include_directories(${IGNITION_ROOT}/projects/common)

add_library(driver_ignition SHARED
  driver.cpp
)

target_link_libraries(driver_ignition PRIVATE ignition_common ignition_bridge_static)
set_target_properties(driver_ignition PROPERTIES LINKER_LANGUAGE CXX)

if (WIN32)
  target_link_libraries(driver_ignition PRIVATE shlwapi)
endif()