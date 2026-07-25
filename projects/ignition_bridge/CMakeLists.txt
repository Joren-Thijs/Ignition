cmake_minimum_required(VERSION 3.16)

project(ignition_bridge)

add_library(ignition_bridge SHARED
    mapped_event_circular_buffer.cpp
    mapped_event_circular_buffer_c.cpp
    shared_memory_block_c.cpp
)

target_compile_definitions(ignition_bridge PUBLIC -DNOMINMAX -D_CRT_SECURE_NO_WARNINGS)
target_include_directories(ignition_bridge PUBLIC ${CMAKE_CURRENT_SOURCE_DIR} ${IGNITION_ROOT}/externals/openvr ${IGNITION_ROOT}/externals/nlohmann)

if (USING_WINE)
    target_compile_definitions(ignition_bridge PRIVATE USING_WINE)
    # Add ignition_bridge.spec to linker input files that so Wine can correctly export our functions.
    target_link_libraries(ignition_bridge PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/ignition_bridge.spec)
endif()

if (NOT WIN32 OR USING_WINE)
    target_link_libraries(ignition_bridge PRIVATE ${CMAKE_DL_LIBS})
endif()