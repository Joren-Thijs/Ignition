# Driver CMake project
cmake_minimum_required(VERSION 3.16)

project(driver_ignition)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/externals/openvr)
include_directories(${CMAKE_SOURCE_DIR}/projects/common)

add_library(${PROJECT_NAME} SHARED
  driver.cpp
)

target_link_libraries(${PROJECT_NAME} PRIVATE ignition_common)

# When done building, copy and replace to C:/Program Files (x86)/Steam/steamapps/common/PlayStation VR2 App/SteamVR_Plug-In/bin/win64/driver_playstation_vr2.dll
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        $<TARGET_FILE:${PROJECT_NAME}>
        "C:/Program Files (x86)/Steam/steamapps/common/PlayStation VR2 App/SteamVR_Plug-In/bin/win64/driver_playstation_vr2.dll"
)
