#pragma once

#include <atomic>
#include <cstddef>
#include <string>

#ifndef __WINE__
#include <windows.h>
#else
#include <semaphore.h>
#endif

namespace ignition {
namespace ipc {

#pragma pack(push, 1)
struct CircularBufferData {
	std::atomic_flag lock = ATOMIC_FLAG_INIT;
	size_t head;
	size_t tail;
	char buffer[];
};
#pragma pack(pop)

class CircularBuffer {
public:
    CircularBuffer();
	~CircularBuffer();

	bool Create(const std::string& name, size_t size);
	bool Open(const std::string& name, size_t size);
	void Close();

    bool write(const char* data, size_t size);
    bool read(char* data, size_t& size);

    void wait_for_data();
private:
	std::string name_;
#ifndef __WINE__
	HANDLE hMapFile_ = NULL;
	HANDLE hDataAvailableEvent_ = NULL;
#else
	int shm_fd_ = -1;
	sem_t *sem_ = nullptr;
#endif
	CircularBufferData* data_ = nullptr;
	size_t buffer_size_ = 0;
};

} // namespace ipc
} // namespace ignition
