# Server CMake project
cmake_minimum_required(VERSION 3.16)

project(ignition_server)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/externals/openvr)
include_directories(${CMAKE_SOURCE_DIR}/externals/nlohmann)
include_directories(${CMAKE_SOURCE_DIR}/projects/common)

# Build as windowed app to not show a console when running under Proton
if (WIN32 AND NOT CMAKE_BUILD_TYPE STREQUAL "Debug")
add_executable(ignition_server WIN32
    main.cpp
)
else()
add_executable(ignition_server
    main.cpp
)
endif()

target_compile_definitions(ignition_server PUBLIC -DNOMINMAX)

target_link_libraries(ignition_server PRIVATE ignition_common ipc_ignition)

if (CMAKE_HOST_WIN32 OR USING_WINE)
    target_link_libraries(ignition_server PRIVATE ole32 shlwapi)
endif()