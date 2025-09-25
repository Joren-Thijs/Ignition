cmake_minimum_required(VERSION 3.10)

project(ipc_ignition)

add_library(ipc_ignition SHARED
    mapped_event_circular_buffer.cpp
    mapped_event_circular_buffer_c.cpp
)

add_library(ipc_ignition_static STATIC
    mapped_event_circular_buffer.cpp
    mapped_event_circular_buffer_c.cpp
)

target_compile_definitions(ipc_ignition PUBLIC -DNOMINMAX)
target_include_directories(ipc_ignition PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(ipc_ignition_static PUBLIC -DNOMINMAX)
target_include_directories(ipc_ignition_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})