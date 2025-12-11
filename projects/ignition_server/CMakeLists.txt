# Server CMake project
cmake_minimum_required(VERSION 3.16)

project(ignition_server)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/externals/openvr)
include_directories(${CMAKE_SOURCE_DIR}/externals/nlohmann)
include_directories(${CMAKE_SOURCE_DIR}/projects/common)

add_executable(ignition_server WIN32
    main.cpp
)

target_compile_definitions(ignition_server PUBLIC -DNOMINMAX)

target_link_libraries(ignition_server PRIVATE ignition_common ipc_ignition)

if (NOT CMAKE_HOST_WIN32 OR CROSS_MINGW)
    # Wine and MinGW need these linked explicitly
    target_link_libraries(ignition_server PRIVATE ole32 shlwapi)
endif()