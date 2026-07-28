#pragma once
#include <string>
#include "shared_memory_block_c.h"

class SharedMemoryBlock {
public:
    SharedMemoryBlock() : bridge_block_(nullptr) {}

    ~SharedMemoryBlock() {
        Close();
    }

    bool Create(const std::string& name, size_t size) {
        Close();
        bridge_block_ = shared_memory_block_create_shm(name.c_str(), size);
        return bridge_block_ != nullptr;
    }

    bool Open(const std::string& name, size_t size) {
        Close();
        bridge_block_ = shared_memory_block_open_shm(name.c_str(), size);
        return bridge_block_ != nullptr;
    }

    void Close() {
        if (bridge_block_) {
            shared_memory_block_close_shm(bridge_block_);
            shared_memory_block_destroy(bridge_block_);
            bridge_block_ = nullptr;
        }
    }

    void* GetPointer() const {
        if (!bridge_block_) return nullptr;
        return shared_memory_block_get_pointer(bridge_block_);
    }

private:
    SharedMemoryBlockBridge* bridge_block_;
};
