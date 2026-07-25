cmake_minimum_required(VERSION 3.16)

project(ignition_server)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${IGNITION_ROOT}/externals/openvr)
include_directories(${IGNITION_ROOT}/externals/nlohmann)
include_directories(${IGNITION_ROOT}/projects/common)

# Pull in dependencies when building standalone (as an ExternalProject).
if(NOT TARGET ignition_common)
    add_subdirectory(${IGNITION_ROOT}/projects/common    ${CMAKE_BINARY_DIR}/common)
endif()
if(NOT TARGET ignition_bridge)
    add_subdirectory(${IGNITION_ROOT}/projects/ignition_bridge ${CMAKE_BINARY_DIR}/ignition_bridge)
endif()

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

target_link_libraries(ignition_server PRIVATE ignition_common ignition_bridge)

if (CMAKE_HOST_WIN32 OR USING_WINE)
    target_link_libraries(ignition_server PRIVATE ole32 shlwapi)
endif()