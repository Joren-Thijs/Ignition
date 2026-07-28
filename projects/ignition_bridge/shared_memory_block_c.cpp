#include "shared_memory_block_c.h"
#include "shared_memory_block.h"

extern "C" {

SharedMemoryBlockBridge* shared_memory_block_create_shm(const char* name, size_t size) {
    auto smb = new ignition::ipc::SharedMemoryBlock();
    if (smb->Create(name, size)) {
        return reinterpret_cast<SharedMemoryBlockBridge*>(smb);
    }
    delete smb;
    return nullptr;
}

SharedMemoryBlockBridge* shared_memory_block_open_shm(const char* name, size_t size) {
    auto smb = new ignition::ipc::SharedMemoryBlock();
    if (smb->Open(name, size)) {
        return reinterpret_cast<SharedMemoryBlockBridge*>(smb);
    }
    delete smb;
    return nullptr;
}

void shared_memory_block_close_shm(SharedMemoryBlockBridge* smb) {
    if (smb) {
        reinterpret_cast<ignition::ipc::SharedMemoryBlock*>(smb)->Close();
    }
}

void shared_memory_block_destroy(SharedMemoryBlockBridge* smb) {
    if (smb) {
        delete reinterpret_cast<ignition::ipc::SharedMemoryBlock*>(smb);
    }
}

void* shared_memory_block_get_pointer(SharedMemoryBlockBridge* smb) {
    if (smb) {
        return reinterpret_cast<ignition::ipc::SharedMemoryBlock*>(smb)->GetPointer();
    }
    return nullptr;
}

}
