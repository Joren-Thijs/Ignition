#pragma once

#include <stddef.h>
#include <stdbool.h>

#ifdef _WIN32
  #define shared_memory_block_API __declspec(dllexport)
#else
  #define shared_memory_block_API
#endif

#ifndef _MSC_VER
#define __cdecl __attribute__((ms_abi))
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SharedMemoryBlockBridge SharedMemoryBlockBridge;

shared_memory_block_API SharedMemoryBlockBridge* __cdecl shared_memory_block_create_shm(const char* name, size_t size);
shared_memory_block_API SharedMemoryBlockBridge* __cdecl shared_memory_block_open_shm(const char* name, size_t size);
shared_memory_block_API void __cdecl shared_memory_block_close_shm(SharedMemoryBlockBridge* smb);
shared_memory_block_API void __cdecl shared_memory_block_destroy(SharedMemoryBlockBridge* smb);
shared_memory_block_API void* __cdecl shared_memory_block_get_pointer(SharedMemoryBlockBridge* smb);

#ifdef __cplusplus
}
#endif
