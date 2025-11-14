cmake_minimum_required(VERSION 3.16)

option(CROSS_MINGW "Cross-compile for Windows using MinGW" OFF)

if (CROSS_MINGW)
    message(STATUS "Cross-compiling for Windows with MinGW toolchain.")
    set(CMAKE_SYSTEM_NAME Windows)
    set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc)
    set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++)
    set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)
    set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
    set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
    set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
    set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)

    # Statically link the MinGW runtime libraries to avoid missing DLL errors.
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static")
    set(CMAKE_SHARED_LINKER_FLAGS "${CMAKE_SHARED_LINKER_FLAGS} -static")
elseif(NOT CMAKE_HOST_WIN32 AND NOT CMAKE_SYSTEM_NAME STREQUAL "Windows")
    message(STATUS "Building on non-Windows platform for Wine. Using wine.cmake toolchain.")
    set(CMAKE_TOOLCHAIN_FILE ${CMAKE_CURRENT_SOURCE_DIR}/wine.cmake)
else()
    message(STATUS "Building on Windows.")
endif()

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