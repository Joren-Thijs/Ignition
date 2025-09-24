cmake_minimum_required(VERSION 3.16)

project(ignition_common)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ignition_common STATIC
    rpc_core.cpp
    vr_rpc_interfaces.cpp
)

target_include_directories(ignition_common PUBLIC 
    ${CMAKE_SOURCE_DIR}/externals/openvr 
    ${CMAKE_CURRENT_SOURCE_DIR}
)
