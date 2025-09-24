cmake_minimum_required(VERSION 3.16)

project(ignition_common)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_library(ignition_common STATIC
    rpc_core.cpp
    rpc_client_context_manager.cpp
    rpc_server_tracked_device_provider.cpp
    rpc_driver_host.cpp
    rpc_driver_log.cpp
    rpc_settings.cpp
    rpc_tracked_device_server_driver.cpp
    rpc_display_component.cpp
    rpc_camera_component.cpp
    rpc_driver_input.cpp
    rpc_driver_manager.cpp
    rpc_properties.cpp
    rpc_resources.cpp
)

target_compile_definitions(ignition_common PUBLIC -DNOMINMAX)

target_include_directories(ignition_common PUBLIC 
    ${CMAKE_SOURCE_DIR}/externals/openvr 
    ${CMAKE_CURRENT_SOURCE_DIR}
)
