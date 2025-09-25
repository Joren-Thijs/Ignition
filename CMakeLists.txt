cmake_minimum_required(VERSION 3.16)

project(ignition)

# Set standard output directories for all targets in this project
# because some projects require files from other projects.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory("projects/ipc_ignition")
add_subdirectory("projects/common")
add_subdirectory("projects/driver_ignition")
add_subdirectory("projects/win32/ignition_server")