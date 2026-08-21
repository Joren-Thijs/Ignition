#include "mapped_event_circular_buffer.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <iostream>

#ifndef _WIN32
#include <cerrno>
#include <fcntl.h>
#include <linux/futex.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

static inline int futex_wait(uint32_t* uaddr, uint32_t val) {
    return syscall(SYS_futex, uaddr, FUTEX_WAIT, val, nullptr, nullptr, 0);
}

static inline int futex_wake(uint32_t* uaddr, int count = 1) {
    return syscall(SYS_futex, uaddr, FUTEX_WAKE, count, nullptr, nullptr, 0);
}
#endif

namespace ignition {
namespace ipc {

namespace {
    void write_data_to_buffer(CircularBufferData* data, size_t& tail, const void* src, size_t count, size_t size) {
        const char* src_bytes = static_cast<const char*>(src);
        size_t first_chunk_size = std::min(count, size - tail);
        memcpy(&data->buffer[tail], src_bytes, first_chunk_size);

        if (first_chunk_size < count) {
            memcpy(&data->buffer[0], src_bytes + first_chunk_size, count - first_chunk_size);
        }
        tail = (tail + count) % size;
    }

    void read_data_from_buffer(const CircularBufferData* data, size_t& head, void* dest, size_t count, size_t size) {
        char* dest_bytes = static_cast<char*>(dest);
        size_t first_chunk_size = std::min(count, size - head);
        memcpy(dest_bytes, &data->buffer[head], first_chunk_size);

        if (first_chunk_size < count) {
            memcpy(dest_bytes + first_chunk_size, &data->buffer[0], count - first_chunk_size);
        }
        head = (head + count) % size;
    }
}

CircularBuffer::CircularBuffer()
	: data_(nullptr) {
}

CircularBuffer::~CircularBuffer() {
	Close();
}

bool CircularBuffer::Create(const std::string& name, size_t size) {
	name_ = name;
	buffer_size_ = size;
	is_creator_ = true;

#ifdef _WIN32
	std::string shm_name = name_ + "_shm";
	std::string event_name = name_ + "_event";

	hMapFile_ = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(CircularBufferData) + size, shm_name.c_str());
	if (hMapFile_ == NULL) {
		std::cerr << "Could not create file mapping object: " << GetLastError() << std::endl;
		return false;
	}

	data_ = (CircularBufferData*)MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CircularBufferData) + size);
	if (data_ == NULL) {
		CloseHandle(hMapFile_);
		hMapFile_ = NULL;
		std::cerr << "Could not map view of file: " << GetLastError() << std::endl;
		return false;
	}

	new (data_) CircularBufferData();
	data_->write_lock.clear(std::memory_order_release);
	data_->head.store(0, std::memory_order_relaxed);
	data_->tail.store(0, std::memory_order_relaxed);

	hDataAvailableEvent_ = CreateEventA(NULL, FALSE, FALSE, event_name.c_str());
	if (hDataAvailableEvent_ == NULL) {
		Close();
		std::cerr << "Failed to create event: " << GetLastError() << std::endl;
		return false;
	}
#else
	std::string shm_name = "/" + name_ + "_shm";

	// Unlink previous instances, in case of a prior unclean exit
	shm_unlink(shm_name.c_str());

	shm_fd_ = shm_open(shm_name.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
	if (shm_fd_ == -1) {
		std::cerr << "Could not create shared memory object: " << errno << std::endl;
		return false;
	}

	if (ftruncate(shm_fd_, sizeof(CircularBufferData) + size) == -1) {
		std::cerr << "Could not set size of shared memory object: " << errno << std::endl;
		close(shm_fd_);
		shm_fd_ = -1;
		return false;
	}

	data_ = (CircularBufferData*)mmap(NULL, sizeof(CircularBufferData) + size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
	if (data_ == MAP_FAILED) {
		std::cerr << "Could not map shared memory object: " << errno << std::endl;
		close(shm_fd_);
		shm_fd_ = -1;
		return false;
	}

	new (data_) CircularBufferData();
	data_->futex_seq = 0;
	data_->write_lock.clear(std::memory_order_release);
	data_->head.store(0, std::memory_order_relaxed);
	data_->tail.store(0, std::memory_order_relaxed);
#endif

	return true;
}

bool CircularBuffer::Open(const std::string& name, size_t size) {
	name_ = name;
	buffer_size_ = size;
	is_creator_ = false;

#ifdef _WIN32
	std::string shm_name = name_ + "_shm";
	std::string event_name = name_ + "_event";

	hMapFile_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name.c_str());
	if (hMapFile_ == NULL) {
		return false;
	}

	data_ = (CircularBufferData*)MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(CircularBufferData) + size);
	if (data_ == NULL) {
		CloseHandle(hMapFile_);
		hMapFile_ = NULL;
		return false;
	}

	hDataAvailableEvent_ = OpenEventA(EVENT_ALL_ACCESS, FALSE, event_name.c_str());
	if (hDataAvailableEvent_ == NULL) {
		Close();
		return false;
	}
#else
	std::string shm_name = "/" + name_ + "_shm";

	shm_fd_ = shm_open(shm_name.c_str(), O_RDWR | O_CLOEXEC, 0);
	if (shm_fd_ == -1) {
		std::cout << "Could not open shared memory object: " << errno << std::endl;
		return false;
	}

	data_ = (CircularBufferData*)mmap(NULL, sizeof(CircularBufferData) + size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
	if (data_ == MAP_FAILED) {
		std::cout << "Could not map shared memory object: " << errno << std::endl;
		close(shm_fd_);
		shm_fd_ = -1;
		return false;
	}
#endif

	return true;
}

void CircularBuffer::Close() {
#ifdef _WIN32
	if (data_) {
		UnmapViewOfFile(data_);
		data_ = nullptr;
	}
	if (hMapFile_) {
		CloseHandle(hMapFile_);
		hMapFile_ = NULL;
	}
	if (hDataAvailableEvent_) {
		CloseHandle(hDataAvailableEvent_);
		hDataAvailableEvent_ = NULL;
	}
#else
	if (data_) {
		munmap(data_, sizeof(CircularBufferData) + buffer_size_);
		data_ = nullptr;
	}
	if (shm_fd_ != -1) {
		close(shm_fd_);
		shm_fd_ = -1;

		if (is_creator_) {
			std::string shm_name = "/" + name_ + "_shm";
			shm_unlink(shm_name.c_str());
		}
	}
#endif
}

bool CircularBuffer::write(const char* data, size_t size) {
	if (!data_ || !data) return false;

	while (data_->write_lock.test_and_set(std::memory_order_acquire)) {}

	const size_t total_size = sizeof(size_t) + size;
	const size_t current_tail = data_->tail.load(std::memory_order_relaxed);
	const size_t current_head = data_->head.load(std::memory_order_acquire);

	size_t free_space;
	if (current_head <= current_tail) {
		free_space = buffer_size_ - (current_tail - current_head);
	}
	else {
		free_space = current_head - current_tail;
	}

	if (total_size >= free_space) {
		data_->write_lock.clear(std::memory_order_release);
		return false; // Not enough space
	}

	size_t next_tail = current_tail;
	write_data_to_buffer(data_, next_tail, &size, sizeof(size_t), buffer_size_);
	write_data_to_buffer(data_, next_tail, data, size, buffer_size_);

	data_->tail.store(next_tail, std::memory_order_release);

	data_->write_lock.clear(std::memory_order_release);

#ifdef _WIN32
	if (hDataAvailableEvent_) {
		SetEvent(hDataAvailableEvent_);
	}
#else
	__atomic_fetch_add(&data_->futex_seq, 1, __ATOMIC_RELEASE);
	futex_wake(&data_->futex_seq, 1);
#endif

	return true;
}

bool CircularBuffer::read(char* data, size_t& size) {
	if (!data_ || !data) return false;

	const size_t current_head = data_->head.load(std::memory_order_relaxed);
	const size_t current_tail = data_->tail.load(std::memory_order_acquire);

	if (current_head == current_tail) {
		return false; // Empty buffer
	}

	size_t message_size = 0;
	size_t next_head = current_head;
	read_data_from_buffer(data_, next_head, &message_size, sizeof(size_t), buffer_size_);

	if (size < message_size) {
		// Provided buffer is too small
		size = 0;
		throw std::runtime_error("Buffer too small for message");
	}

	read_data_from_buffer(data_, next_head, data, message_size, buffer_size_);
	size = message_size;

	data_->head.store(next_head, std::memory_order_release);
	return true;
}

void CircularBuffer::wait_for_data() {
#ifdef _WIN32
	if (hDataAvailableEvent_)
		WaitForSingleObject(hDataAvailableEvent_, INFINITE);
#else
	if (!data_) return;

	while (data_->head.load(std::memory_order_relaxed) == data_->tail.load(std::memory_order_acquire)) {
		uint32_t expected = __atomic_load_n(&data_->futex_seq, __ATOMIC_ACQUIRE);

		// If we still have data, don't wait
		if (data_->head.load(std::memory_order_relaxed) != data_->tail.load(std::memory_order_acquire)) {
			break;
		}

		int ret = futex_wait(&data_->futex_seq, expected);
		if (ret == -1 && (errno == EAGAIN || errno == EINTR)) {
			continue;
		}
	}
#endif
}

} // namespace ipc
} // namespace ignition
