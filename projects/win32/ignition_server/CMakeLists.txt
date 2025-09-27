# Server CMake project
cmake_minimum_required(VERSION 3.16)

project(ignition_server)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/externals/openvr)
include_directories(${CMAKE_SOURCE_DIR}/projects/common)

add_executable(ignition_server
    main.cpp
)

target_compile_definitions(ignition_server PUBLIC -DNOMINMAX)

target_link_libraries(ignition_server PRIVATE ignition_common ipc_ignition)

if (USING_WINE)
    # Wine needs ole32 linked
    target_link_libraries(ignition_server PRIVATE ole32)
endif()