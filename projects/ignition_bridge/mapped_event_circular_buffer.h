#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace ignition {
namespace ipc {

struct CircularBufferData {
#ifndef _WIN32
	alignas(4) uint32_t futex_seq{0};
#endif
	std::atomic_flag write_lock = ATOMIC_FLAG_INIT;
	alignas(64) std::atomic<size_t> head{0};
	alignas(64) std::atomic<size_t> tail{0};
	alignas(64) char buffer[];
};

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
	bool is_creator_ = false;
#ifdef _WIN32
	HANDLE hMapFile_ = NULL;
	HANDLE hDataAvailableEvent_ = NULL;
#else
	int shm_fd_ = -1;
#endif
	CircularBufferData* data_ = nullptr;
	size_t buffer_size_ = 0;
};

} // namespace ipc
} // namespace ignition

