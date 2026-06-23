#include "mapped_event_circular_buffer_c.h"
#include "mapped_event_circular_buffer.h"

extern "C" {

CircularBuffer* mapped_event_circular_buffer_create_shm(const char* name, size_t size) {
    auto cb = new ignition::ipc::CircularBuffer();
    if (cb->Create(name, size)) {
        return reinterpret_cast<CircularBuffer*>(cb);
    }
    delete cb;
    return nullptr;
}

CircularBuffer* mapped_event_circular_buffer_open_shm(const char* name, size_t size) {
    auto cb = new ignition::ipc::CircularBuffer();
    if (cb->Open(name, size)) {
        return reinterpret_cast<CircularBuffer*>(cb);
    }
    delete cb;
    return nullptr;
}

void mapped_event_circular_buffer_close_shm(CircularBuffer* cb) {
    reinterpret_cast<ignition::ipc::CircularBuffer*>(cb)->Close();
}

CircularBuffer* mapped_event_circular_buffer_create() {
    return reinterpret_cast<CircularBuffer*>(new ignition::ipc::CircularBuffer());
}

void mapped_event_circular_buffer_destroy(CircularBuffer* cb) {
    delete reinterpret_cast<ignition::ipc::CircularBuffer*>(cb);
}

bool mapped_event_circular_buffer_write(CircularBuffer* cb, const char* data, size_t size) {
    return reinterpret_cast<ignition::ipc::CircularBuffer*>(cb)->write(data, size);
}

bool mapped_event_circular_buffer_read(CircularBuffer* cb, char* data, size_t* size) {
    return reinterpret_cast<ignition::ipc::CircularBuffer*>(cb)->read(data, *size);
}

void mapped_event_circular_buffer_wait_for_data(CircularBuffer* cb) {
    reinterpret_cast<ignition::ipc::CircularBuffer*>(cb)->wait_for_data();
}

}
