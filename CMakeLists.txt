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

set(IGNITION_ROOT "${CMAKE_SOURCE_DIR}")

if(NOT CMAKE_HOST_WIN32)
    find_file(WINEBUILD_PATH 
        NAMES winebuild
        ENV PATH
    )

    if(WINEBUILD_PATH)
        message("Winebuild found at ${WINEBUILD_PATH}")
        set(HAS_WINEBUILD 1)
    endif()
endif()

add_subdirectory("projects/common")
add_subdirectory("projects/driver_ignition")
add_subdirectory("projects/ignition_bridge")
add_subdirectory("projects/ignition_server")

if(NOT CMAKE_HOST_WIN32)
    include(ExternalProject)

    # Build server as separate project to make the Windows binary
    ExternalProject_Add(
        ignition_server_windows
        SOURCE_DIR      "${CMAKE_CURRENT_SOURCE_DIR}/projects/ignition_server"
        BINARY_DIR      "${CMAKE_CURRENT_BINARY_DIR}/ignition_server_build"
        INSTALL_DIR     "${CMAKE_BINARY_DIR}/Windows/bin"
        CMAKE_ARGS      -DCMAKE_INSTALL_PREFIX=<INSTALL_DIR>
                        -DCMAKE_BUILD_TYPE=${CMAKE_BUILD_TYPE}
                        -DCMAKE_TRY_COMPILE_CONFIGURATION=Release
                        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_CURRENT_SOURCE_DIR}/clang-x86-64-pc-windows-msvc.cmake
                        -DIGNITION_ROOT=${CMAKE_CURRENT_SOURCE_DIR}
        BUILD_ALWAYS    1
        INSTALL_COMMAND ${CMAKE_COMMAND} -E copy <BINARY_DIR>/ignition_server.exe ${CMAKE_BINARY_DIR}/Windows/bin/ignition_server.exe
    )

    set(PACKAGE_DIR ${CMAKE_BINARY_DIR}/Ignition-${CMAKE_SYSTEM_NAME}-Windows)

    add_custom_target(package_ignition ALL
        COMMAND ${CMAKE_COMMAND} -E make_directory ${PACKAGE_DIR}

        COMMAND ${CMAKE_COMMAND} -E copy_directory ${CMAKE_CURRENT_SOURCE_DIR}/support ${PACKAGE_DIR}
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${CMAKE_BINARY_DIR}/Windows/bin/ignition_server.exe ${PACKAGE_DIR}/ignition_server.exe
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:ignition_bridge> ${PACKAGE_DIR}/ignition_bridge.dll
        COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:driver_ignition> ${PACKAGE_DIR}/${CMAKE_SHARED_LIBRARY_PREFIX}driver_ignition${CMAKE_SHARED_LIBRARY_SUFFIX}

        COMMENT "Packaging Ignition files into ${PACKAGE_DIR}"
    )

    add_dependencies(package_ignition ignition_server_windows ignition_bridge driver_ignition)
endif()