cmake_minimum_required(VERSION 3.10)

project(ignition_bridge)

add_library(ignition_bridge SHARED
    mapped_event_circular_buffer.cpp
    mapped_event_circular_buffer_c.cpp
)

add_library(ignition_bridge_static STATIC
    mapped_event_circular_buffer.cpp
    mapped_event_circular_buffer_c.cpp
)

target_compile_definitions(ignition_bridge PUBLIC -DNOMINMAX)
target_include_directories(ignition_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_compile_definitions(ignition_bridge_static PUBLIC -DNOMINMAX)
target_include_directories(ignition_bridge_static PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

if (USING_WINE)
    # Add ignition_bridge.spec to linker input files that so Wine can correctly export our functions.
    target_link_libraries(ignition_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ignition_bridge.spec)
endif()