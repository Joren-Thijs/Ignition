#pragma once

#include <cstdint>
#include <stddef.h>
#include <stdbool.h>

// DLL export
#ifndef __WINE__
  #define mapped_event_circular_buffer_API __declspec(dllexport)
#else
  #define mapped_event_circular_buffer_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct CircularBuffer CircularBuffer;

mapped_event_circular_buffer_API CircularBuffer* __cdecl mapped_event_circular_buffer_create_shm(const char* name, size_t size);
mapped_event_circular_buffer_API CircularBuffer* __cdecl mapped_event_circular_buffer_open_shm(const char* name, size_t size);
mapped_event_circular_buffer_API void __cdecl mapped_event_circular_buffer_close_shm(CircularBuffer* cb);

mapped_event_circular_buffer_API CircularBuffer* __cdecl mapped_event_circular_buffer_create();
mapped_event_circular_buffer_API void __cdecl mapped_event_circular_buffer_destroy(CircularBuffer* cb);

mapped_event_circular_buffer_API bool __cdecl mapped_event_circular_buffer_write(CircularBuffer* cb, const char* data, size_t size);
mapped_event_circular_buffer_API bool __cdecl mapped_event_circular_buffer_read(CircularBuffer* cb, char* data, size_t* size);

mapped_event_circular_buffer_API void __cdecl mapped_event_circular_buffer_wait_for_data(CircularBuffer* cb, uint32_t timeout_ms);

#ifdef __cplusplus
}
#endif
