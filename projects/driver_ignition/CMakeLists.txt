# Driver CMake project
cmake_minimum_required(VERSION 3.16)

project(driver_ignition)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(${CMAKE_SOURCE_DIR}/externals/openvr)
include_directories(${CMAKE_SOURCE_DIR}/projects/common)

add_library(driver_ignition SHARED
  driver.cpp
)

target_link_libraries(driver_ignition PRIVATE ignition_common ipc_ignition_static)

set_target_properties(driver_ignition PROPERTIES LINKER_LANGUAGE CXX)

if (CMAKE_HOST_WIN32)
  # When done building, copy and replace to C:/Program Files (x86)/Steam/steamapps/common/PlayStation VR2 App/SteamVR_Plug-In/bin/win64/driver_playstation_vr2.dll
  add_custom_command(TARGET driver_ignition POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
          $<TARGET_FILE:driver_ignition>
          "C:/Program Files (x86)/Steam/steamapps/common/PlayStation VR2 App/SteamVR_Plug-In/bin/win64/driver_playstation_vr2.dll"
  )
endif()