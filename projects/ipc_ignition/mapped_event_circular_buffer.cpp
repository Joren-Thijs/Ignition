#include "mapped_event_circular_buffer.h"
#include <algorithm>
#include <stdexcept>
#include <iostream>

#ifndef _WIN32
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
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
	std::string shm_name = name_ + "_shm";
	std::string event_name = name_ + "_event";

#ifdef _WIN32
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
	data_->head = 0;
	data_->tail = 0;
	data_->lock.clear(std::memory_order_release);

	hDataAvailableEvent_ = CreateEventA(NULL, FALSE, FALSE, event_name.c_str());
	if (hDataAvailableEvent_ == NULL) {
		Close();
		std::cerr << "Failed to create event: " << GetLastError() << std::endl;
		return false;
	}
#else
	// Unlink previous instances, in case of a crash
	shm_unlink(shm_name.c_str());
	sem_unlink(event_name.c_str());

	shm_fd_ = shm_open(shm_name.c_str(), O_CREAT | O_RDWR, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
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
	data_->head = 0;
	data_->tail = 0;
	data_->lock.clear(std::memory_order_release);

	sem_ = sem_open(event_name.c_str(), O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH, 0);
	if (sem_ == SEM_FAILED) {
		std::cerr << "Failed to create semaphore: " << errno << std::endl;
		munmap(data_, sizeof(CircularBufferData) + size);
		data_ = nullptr;
		close(shm_fd_);
		shm_fd_ = -1;
		return false;
	}
#endif

	return true;
}

bool CircularBuffer::Open(const std::string& name, size_t size) {
	name_ = name;
	buffer_size_ = size;
	std::string shm_name = name_ + "_shm";
	std::string event_name = name_ + "_event";

#ifdef _WIN32
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
	shm_fd_ = shm_open(shm_name.c_str(), O_RDWR, 0);
	if (shm_fd_ == -1) {
		return false;
	}

	data_ = (CircularBufferData*)mmap(NULL, sizeof(CircularBufferData) + size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
	if (data_ == MAP_FAILED) {
		close(shm_fd_);
		shm_fd_ = -1;
		return false;
	}

	sem_ = sem_open(event_name.c_str(), O_RDWR);
	if (sem_ == SEM_FAILED) {
		munmap(data_, sizeof(CircularBufferData) + size);
		data_ = nullptr;
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
	if (sem_) {
		sem_close(sem_);
		sem_ = nullptr;
	}
	if (shm_fd_ != -1) {
		close(shm_fd_);
		shm_fd_ = -1;

		std::string shm_name = name_ + "_shm";
		std::string event_name = name_ + "_event";
		shm_unlink(shm_name.c_str());
		sem_unlink(event_name.c_str());
	}
#endif
}

bool CircularBuffer::write(const char* data, size_t size) {
	if (!data_) return false;

	while (data_->lock.test_and_set(std::memory_order_acquire)) {}

	const size_t total_size = sizeof(size_t) + size;
	size_t free_space;
	if (data_->head <= data_->tail) {
		free_space = buffer_size_ - (data_->tail - data_->head);
	}
	else {
		free_space = data_->head - data_->tail;
	}

	if (total_size >= free_space) {
		data_->lock.clear(std::memory_order_release);
		return false; // Not enough space
	}

	size_t current_tail = data_->tail;
	write_data_to_buffer(data_, current_tail, &size, sizeof(size_t), buffer_size_);
	write_data_to_buffer(data_, current_tail, data, size, buffer_size_);
	data_->tail = current_tail;

	data_->lock.clear(std::memory_order_release);

#ifdef _WIN32
	SetEvent(hDataAvailableEvent_);
#else
	sem_post(sem_);
#endif

	return true;
}

bool CircularBuffer::read(char* data, size_t& size) {
	if (!data_) return false;

	while (data_->lock.test_and_set(std::memory_order_acquire)) {}

	bool hasRead = false;
	if (data_->head != data_->tail) {
		size_t message_size;
		size_t current_head = data_->head;
		read_data_from_buffer(data_, current_head, &message_size, sizeof(size_t), buffer_size_);

		if (size >= message_size) {
			read_data_from_buffer(data_, current_head, data, message_size, buffer_size_);
			
			data_->head = current_head;
			size = message_size;
			hasRead = true;
		}
		else {
			// Provided buffer is too small
			size = 0;
			throw std::runtime_error("Buffer too small for message");
		}
	}

	data_->lock.clear(std::memory_order_release);
	return hasRead;
}

void CircularBuffer::wait_for_data() {
#ifdef _WIN32
	if (hDataAvailableEvent_)
		WaitForSingleObject(hDataAvailableEvent_, INFINITE);
#else
	if (sem_)
		sem_wait(sem_);
#endif
}

} // namespace ipc
} // namespace ignition
