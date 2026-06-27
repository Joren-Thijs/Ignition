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
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${CMAKE_SYSTEM_NAME}/bin)

add_subdirectory("projects/common")
add_subdirectory("projects/driver_ignition")
add_subdirectory("projects/ignition_bridge")
add_subdirectory("projects/ignition_server")

if(NOT CMAKE_HOST_WIN32)
    # Also build IPC as separate project to make the winedll
    include(ExternalProject)
    ExternalProject_Add(
        ignition_bridge_wine
        SOURCE_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/projects/ignition_bridge"
        BINARY_DIR      "${CMAKE_CURRENT_BINARY_DIR}/ignition_bridge_build"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/WineDLL"
        CMAKE_ARGS      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/wine.cmake
        BUILD_ALWAYS    1
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/libignition_bridge.so ${CMAKE_BINARY_DIR}/WineDLL/ignition_bridge.dll
    )
endif()

set(PACKAGE_DIR ${CMAKE_BINARY_DIR}/Ignition-Linux-Windows)

add_custom_target(package_ignition ALL
    COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGE_DIR}

    # Copy support folder
    COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/support ${PACKAGE_DIR}

    # Copy ignition_server.exe
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/Windows/bin/ignition_server.exe ${PACKAGE_DIR}/ignition_server.exe

    # Copy ignition_bridge.dll
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/WineDLL/ignition_bridge.dll ${PACKAGE_DIR}/ignition_bridge.dll

    # Copy libdriver_ignition.so
    COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/Linux/lib/libdriver_ignition.so ${PACKAGE_DIR}/libdriver_ignition.so

    COMMENT "Packaging Ignition files into ${PACKAGE_DIR}"
)

# Add checks to warn if files are missing during configuration.
if(NOT EXISTS "${CMAKE_BINARY_DIR}/Windows/bin/ignition_server.exe")
    message(WARNING "ignition_server.exe not found. It will not be packaged.")
endif()
if(NOT EXISTS "${CMAKE_BINARY_DIR}/WineDLL/ignition_bridge.dll")
    message(WARNING "ignition_bridge.dll not found. It will not be packaged.")
endif()
if(NOT EXISTS "${CMAKE_BINARY_DIR}/Linux/lib/libdriver_ignition.so")
    message(WARNING "libdriver_ignition.so not found. It will not be packaged.")
endif()