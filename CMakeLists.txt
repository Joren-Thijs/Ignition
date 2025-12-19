cmake_minimum_required(VERSION 3.16)

project(ignition)

# If we're compiling for Windows, it cannot be MinGW due to the ABI.
if(WIN32 AND CMAKE_CXX_COMPILER_ID MATCHES "GNU")
    message(FATAL_ERROR "MinGW is not supported for Windows builds. Please use MSVC or Clang-MSVC.")
endif()

if(NOT WIN32)
    set(CMAKE_POSITION_INDEPENDENT_CODE ON)
endif()

# Set standard output directories for all targets in this project
# because some projects require files from other projects.
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_subdirectory("projects/ipc_ignition")
add_subdirectory("projects/common")
add_subdirectory("projects/driver_ignition")
add_subdirectory("projects/ignition_server")

if(NOT CMAKE_HOST_WIN32)
    # Also build IPC as separate project to make the winedll
    include(ExternalProject)
    ExternalProject_Add(
        ipc_ignition_wine
        SOURCE_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/projects/ipc_ignition"
        BINARY_DIR      "${CMAKE_CURRENT_BINARY_DIR}/ipc_ignition_build"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/lib"
        CMAKE_ARGS      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/wine.cmake
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libipc_ignition.so ${CMAKE_BINARY_DIR}/lib/ipc_ignition.dll
    )
endif()