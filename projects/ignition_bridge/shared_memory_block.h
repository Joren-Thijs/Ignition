#pragma once
#include <string>

#if defined(_WIN32) && !USING_WINE
#include <windows.h>
#else
#include <sys/mman.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace ignition {
namespace ipc {

class SharedMemoryBlock {
public:
    SharedMemoryBlock() : data_(nullptr), size_(0) {
#if defined(_WIN32) && !USING_WINE
        hMapFile_ = NULL;
#else
        shm_fd_ = -1;
#endif
    }

    ~SharedMemoryBlock() {
        Close();
    }

    bool Create(const std::string& name, size_t size) {
        name_ = name;
        size_ = size;
#if defined(_WIN32) && !USING_WINE
        std::string shm_name = name_ + "_shm";
        hMapFile_ = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, (DWORD)size, shm_name.c_str());
        if (hMapFile_ == NULL) return false;
        data_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (data_ == NULL) {
            CloseHandle(hMapFile_);
            hMapFile_ = NULL;
            return false;
        }
#else
        std::string shm_name = "/" + name_ + "_shm";
        shm_unlink(shm_name.c_str());
        shm_fd_ = shm_open(shm_name.c_str(), O_CREAT | O_RDWR | O_CLOEXEC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);
        if (shm_fd_ == -1) return false;
        if (ftruncate(shm_fd_, size) == -1) {
            close(shm_fd_);
            shm_fd_ = -1;
            return false;
        }
        data_ = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            close(shm_fd_);
            shm_fd_ = -1;
            return false;
        }
#endif
        return true;
    }

    bool Open(const std::string& name, size_t size) {
        name_ = name;
        size_ = size;
#if defined(_WIN32) && !USING_WINE
        std::string shm_name = name_ + "_shm";
        hMapFile_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name.c_str());
        if (hMapFile_ == NULL) return false;
        data_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, size);
        if (data_ == NULL) {
            CloseHandle(hMapFile_);
            hMapFile_ = NULL;
            return false;
        }
#else
        std::string shm_name = "/" + name_ + "_shm";
        shm_fd_ = shm_open(shm_name.c_str(), O_RDWR | O_CLOEXEC, 0);
        if (shm_fd_ == -1) return false;
        data_ = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, shm_fd_, 0);
        if (data_ == MAP_FAILED) {
            data_ = nullptr;
            close(shm_fd_);
            shm_fd_ = -1;
            return false;
        }
#endif
        return true;
    }

    void Close() {
#if defined(_WIN32) && !USING_WINE
        if (data_) {
            UnmapViewOfFile(data_);
            data_ = nullptr;
        }
        if (hMapFile_) {
            CloseHandle(hMapFile_);
            hMapFile_ = NULL;
        }
#else
        if (data_) {
            munmap(data_, size_);
            data_ = nullptr;
        }
        if (shm_fd_ != -1) {
            close(shm_fd_);
            shm_fd_ = -1;
            std::string shm_name = "/" + name_ + "_shm";
            shm_unlink(shm_name.c_str());
        }
#endif
    }

    void* GetPointer() const { return data_; }

private:
    std::string name_;
    size_t size_;
    void* data_;
#if defined(_WIN32) && !USING_WINE
    HANDLE hMapFile_;
#else
    int shm_fd_;
#endif
};

} // namespace ipc
} // namespace ignition
