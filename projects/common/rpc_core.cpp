#include "rpc_core.h"
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <vector>

// --- Static Member Definitions for RpcSystem ---
std::string RpcSystem::pipe_name_;
bool RpcSystem::is_server_ = false;
std::unique_ptr<std::thread> RpcSystem::listen_thread_;
bool RpcSystem::running_ = false;

HANDLE RpcSystem::hMapFile_ = NULL;
void* RpcSystem::pSharedMem_ = nullptr;
ignition::rpc::CircularBuffer* RpcSystem::pC2S_Buffer_ = nullptr;
ignition::rpc::CircularBuffer* RpcSystem::pS2C_Buffer_ = nullptr;
HANDLE RpcSystem::hC2S_DataAvailableEvent_ = NULL;
HANDLE RpcSystem::hS2C_DataAvailableEvent_ = NULL;


std::map<std::string, RpcSystem::RpcFunction> RpcSystem::function_registry_;
std::mutex RpcSystem::registry_mutex_;

std::atomic<RpcObjectId> RpcSystem::next_object_id_{1};
std::map<RpcObjectId, RpcObject*> RpcSystem::local_objects_;
std::map<RpcObjectId, std::unique_ptr<RpcObject>> RpcSystem::remote_proxies_;
std::map<std::string, RpcSystem::ObjectFactory> RpcSystem::object_factories_;
std::mutex RpcSystem::object_mutex_;

std::map<uint32_t, std::shared_ptr<RpcSystem::PendingCall>> RpcSystem::pending_calls_;
std::mutex RpcSystem::pending_calls_mutex_;
std::atomic<uint32_t> RpcSystem::next_call_id_ = 1;

std::mutex RpcSystem::send_mutex_;
std::vector<std::thread> RpcSystem::worker_threads_;
std::queue<std::function<void()>> RpcSystem::tasks_;
std::mutex RpcSystem::thread_pool_mutex_;
std::condition_variable RpcSystem::thread_pool_cv_;
bool RpcSystem::stop_thread_pool_ = false;
std::mutex RpcSystem::cout_mutex_;


// --- RpcValue Implementation ---
RpcValue::RpcValue(RpcObject* v) : type_(T_OBJECT_REF) {
    assert(v != nullptr && "Cannot construct RpcValue with a null RpcObject*");
    obj_ref_.id = v->GetId();
    obj_ref_.class_name = &v->GetRpcClassName(); // This was missing
}

RpcValue::RpcValue(const RpcValue& other) : type_(other.type_) {
    switch (type_) {
        case T_INT:        int_val_    = other.int_val_;    break;
        case T_FLOAT:      float_val_  = other.float_val_;  break;
        case T_DOUBLE:     double_val_ = other.double_val_; break;
        case T_UINT64:     uint64_val_ = other.uint64_val_; break;
        case T_OBJECT_REF: obj_ref_    = other.obj_ref_;    break;
        case T_STRING:     str_val_    = other.str_val_;    break;
        case T_POINTER:    ptr_val_    = other.ptr_val_;    break;
        case T_NULL:       /* Nothing to copy */            break;
    }
}

RpcValue& RpcValue::operator=(const RpcValue& other) {
    RpcValue(other).swap(*this);
    return *this;
}

void RpcValue::swap(RpcValue& other) noexcept {
    using std::swap;
    swap(type_, other.type_);
    swap(str_val_, other.str_val_);
    swap(ptr_val_, other.ptr_val_);

    // The union can be swapped safely with a temporary buffer
    // that is large enough to hold the largest member.
    char buffer[sizeof(obj_ref_)];
    memcpy(buffer, &other.int_val_, sizeof(buffer));
    memcpy(&other.int_val_, &this->int_val_, sizeof(buffer));
    memcpy(&this->int_val_, buffer, sizeof(buffer));
}

RpcValue::~RpcValue() = default;

int RpcValue::asInt() const {
    if (!isInt()) throw std::runtime_error("RpcValue is not an int.");
    return int_val_;
}
float RpcValue::asFloat() const {
    if (!isFloat()) throw std::runtime_error("RpcValue is not a float.");
    return float_val_;
}
double RpcValue::asDouble() const {
    if (!isDouble()) throw std::runtime_error("RpcValue is not a double.");
    return double_val_;
}
std::string RpcValue::asString() const {
    if (!isString()) throw std::runtime_error("RpcValue is not a string.");
    return str_val_;
}
uint64_t RpcValue::asUint64() const {
    if (!isUint64())
    {
        throw std::runtime_error("RpcValue is not a uint64.");
    }
    return uint64_val_;
}
std::pair<const char*, size_t> RpcValue::asPointer() const {
    if (!isPointer()) throw std::runtime_error("RpcValue is not a pointer.");
    return {ptr_val_.data(), ptr_val_.size()};
}
RpcObject* RpcValue::asObject() const {
    if (!isObject()) throw std::runtime_error("RpcValue is not an object reference.");
    return RpcSystem::FindOrCreateProxy(obj_ref_.id, *obj_ref_.class_name);
}

// --- RpcObject Implementation ---
RpcObject::RpcObject() : is_proxy_(false) {
    object_id_ = RpcSystem::GenerateObjectId();
    RpcSystem::RegisterLocalObject(this);
}

RpcObject::RpcObject(RpcObjectId id) : object_id_(id), is_proxy_(true) {}

RpcObject::~RpcObject() {
    if (!is_proxy_) {
        RpcSystem::UnregisterLocalObject(object_id_);
    }
}


// --- Serializer Implementation ---
void Serializer::serialize(std::vector<char>& buffer, const RpcValue& val) {
    buffer.push_back(static_cast<char>(val.type_));
    switch (val.type_) {
        case RpcValue::T_INT: {
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val.int_val_), reinterpret_cast<const char*>(&val.int_val_) + sizeof(int));
            break;
        }
        case RpcValue::T_FLOAT: {
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val.float_val_), reinterpret_cast<const char*>(&val.float_val_) + sizeof(float));
            break;
        }
        case RpcValue::T_DOUBLE: {
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val.double_val_), reinterpret_cast<const char*>(&val.double_val_) + sizeof(double));
            break;
        }
        case RpcValue::T_UINT64: {
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val.uint64_val_), reinterpret_cast<const char*>(&val.uint64_val_) + sizeof(uint64_t));
            break;
        }
        case RpcValue::T_STRING:
        case RpcValue::T_POINTER: {
            const auto& data_vec = (val.type_ == RpcValue::T_STRING) ?
                std::vector<char>(val.str_val_.begin(), val.str_val_.end()) : val.ptr_val_;
            uint32_t len = data_vec.size();
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&len), reinterpret_cast<const char*>(&len) + sizeof(uint32_t));
            buffer.insert(buffer.end(), data_vec.begin(), data_vec.end());
            break;
        }
        case RpcValue::T_OBJECT_REF: {
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val.obj_ref_.id), reinterpret_cast<const char*>(&val.obj_ref_.id) + sizeof(RpcObjectId));
            uint32_t name_len = val.obj_ref_.class_name->length();
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&name_len), reinterpret_cast<const char*>(&name_len) + sizeof(uint32_t));
            buffer.insert(buffer.end(), val.obj_ref_.class_name->begin(), val.obj_ref_.class_name->end());
            break;
        }
        case RpcValue::T_NULL:
            break;
    }
}

RpcValue Serializer::deserialize(const char*& buffer_ptr, const char* buffer_end) {
    if (buffer_ptr >= buffer_end) throw std::runtime_error("Deserialization buffer underflow.");
    
    RpcValue::Type type = static_cast<RpcValue::Type>(*buffer_ptr++);
    
    switch (type) {
        case RpcValue::T_INT: {
            int val;
            memcpy(&val, buffer_ptr, sizeof(int));
            buffer_ptr += sizeof(int);
            return RpcValue(val);
        }
        case RpcValue::T_FLOAT: {
            float val;
            memcpy(&val, buffer_ptr, sizeof(float));
            buffer_ptr += sizeof(float);
            return RpcValue(val);
        }
        case RpcValue::T_DOUBLE: {
            double val;
            memcpy(&val, buffer_ptr, sizeof(double));
            buffer_ptr += sizeof(double);
            return RpcValue(val);
        }
        case RpcValue::T_UINT64: {
            uint64_t val;
            memcpy(&val, buffer_ptr, sizeof(uint64_t));
            buffer_ptr += sizeof(uint64_t);
            return RpcValue(val);
        }
        case RpcValue::T_STRING: {
            uint32_t len;
            memcpy(&len, buffer_ptr, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            std::string s(buffer_ptr, len);
            buffer_ptr += len;
            return RpcValue(s);
        }
        case RpcValue::T_POINTER: {
            uint32_t len;
            memcpy(&len, buffer_ptr, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            RpcValue v(buffer_ptr, len);
            buffer_ptr += len;
            return v;
        }
        case RpcValue::T_OBJECT_REF: {
            RpcObjectId id;
            memcpy(&id, buffer_ptr, sizeof(RpcObjectId));
            buffer_ptr += sizeof(RpcObjectId);

            uint32_t name_len;
            memcpy(&name_len, buffer_ptr, sizeof(uint32_t));
            buffer_ptr += sizeof(uint32_t);
            std::string class_name(buffer_ptr, name_len);
            buffer_ptr += name_len;
            
            RpcObject* obj = RpcSystem::FindOrCreateProxy(id, class_name);
            return RpcValue(obj);
        }
        case RpcValue::T_NULL:
            return RpcValue();
        default:
            throw std::runtime_error("Unknown type during deserialization.");
    }
}

// --- RpcSystem Static Method Implementations ---
void RpcSystem::Initialize(const std::string& pipeName) {
    pipe_name_ = pipeName;
    next_object_id_ = 1;
    next_call_id_ = 1;
    running_ = true;
    InitializeThreadPool(5);
}

void RpcSystem::Shutdown() {
    running_ = false;

    // Wake up the listener thread so it can exit
    if (hC2S_DataAvailableEvent_) SetEvent(hC2S_DataAvailableEvent_);
    if (hS2C_DataAvailableEvent_) SetEvent(hS2C_DataAvailableEvent_);

    ShutdownThreadPool();

    if (listen_thread_ && listen_thread_->joinable()) {
        listen_thread_->join();
    }
    // Cleanup shared memory and sync objects
    if (pSharedMem_) UnmapViewOfFile(pSharedMem_);
    if (hMapFile_) CloseHandle(hMapFile_);
    if (hC2S_DataAvailableEvent_) CloseHandle(hC2S_DataAvailableEvent_);
    if (hS2C_DataAvailableEvent_) CloseHandle(hS2C_DataAvailableEvent_);

    pSharedMem_ = nullptr;
    hMapFile_ = NULL;
    hC2S_DataAvailableEvent_ = NULL;
    hS2C_DataAvailableEvent_ = NULL;
    pC2S_Buffer_ = nullptr;
    pS2C_Buffer_ = nullptr;

    // Clear out proxies to break potential circular references
    remote_proxies_.clear();
}

void RpcSystem::InitializeThreadPool(size_t num_threads) {
    stop_thread_pool_ = false;
    for (size_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back([] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(thread_pool_mutex_);
                    thread_pool_cv_.wait(lock, [] { return stop_thread_pool_ || !tasks_.empty(); });
                    if (stop_thread_pool_ && tasks_.empty()) {
                        return;
                    }
                    task = std::move(tasks_.front());
                    tasks_.pop();
                }
                task();
            }
        });
    }
}

void RpcSystem::ShutdownThreadPool() {
    {
        std::unique_lock<std::mutex> lock(thread_pool_mutex_);
        stop_thread_pool_ = true;
    }

    thread_pool_cv_.notify_all();
    for (std::thread& worker : worker_threads_) {
        worker.join();
    }
}

bool RpcSystem::IsConnected() {
    return pSharedMem_ != nullptr;
}

void RpcSystem::RegisterFunction(const std::string& name, RpcFunction func) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    function_registry_[name] = func;
}

void RpcSystem::UnregisterFunction(const std::string& name) {
     std::lock_guard<std::mutex> lock(registry_mutex_);
     function_registry_.erase(name);
}

void RpcSystem::StartServer() {
    is_server_ = true;
    std::string shm_name = pipe_name_ + "_shm";
    std::string c2s_event_name = pipe_name_ + "_c2s_event";
    std::string s2c_event_name = pipe_name_ + "_s2c_event";

    hMapFile_ = CreateFileMappingA(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, sizeof(ignition::rpc::CircularBuffer) * 2, shm_name.c_str());
    if (hMapFile_ == NULL) throw std::runtime_error("Could not create file mapping object.");

    pSharedMem_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ignition::rpc::CircularBuffer) * 2);
    if (pSharedMem_ == NULL) {
        CloseHandle(hMapFile_);
        throw std::runtime_error("Could not map view of file.");
    }

    pC2S_Buffer_ = static_cast<ignition::rpc::CircularBuffer*>(pSharedMem_);
    pS2C_Buffer_ = pC2S_Buffer_ + 1;

    // Initialize circular buffers
    new (pC2S_Buffer_) ignition::rpc::CircularBuffer();
    new (pS2C_Buffer_) ignition::rpc::CircularBuffer();

    hC2S_DataAvailableEvent_ = CreateEventA(NULL, FALSE, FALSE, c2s_event_name.c_str());
    hS2C_DataAvailableEvent_ = CreateEventA(NULL, FALSE, FALSE, s2c_event_name.c_str());

    if (!hC2S_DataAvailableEvent_ || !hS2C_DataAvailableEvent_) {
        Shutdown();
        throw std::runtime_error("Failed to create synchronization objects.");
    }

    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "Shared memory server running. Waiting for client..." << std::endl;
    }

    listen_thread_ = std::make_unique<std::thread>(&RpcSystem::ListenLoop);
}

bool RpcSystem::ConnectToServer() {
    is_server_ = false;
    std::string shm_name = pipe_name_ + "_shm";
    std::string c2s_event_name = pipe_name_ + "_c2s_event";
    std::string s2c_event_name = pipe_name_ + "_s2c_event";

    hMapFile_ = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, shm_name.c_str());
    if (hMapFile_ == NULL) {
        std::cerr << "Could not open file mapping object." << std::endl;
        return false;
    }

    pSharedMem_ = MapViewOfFile(hMapFile_, FILE_MAP_ALL_ACCESS, 0, 0, sizeof(ignition::rpc::CircularBuffer) * 2);
    if (pSharedMem_ == NULL) {
        CloseHandle(hMapFile_);
        std::cerr << "Could not map view of file." << std::endl;
        return false;
    }

    pC2S_Buffer_ = static_cast<ignition::rpc::CircularBuffer*>(pSharedMem_);
    pS2C_Buffer_ = pC2S_Buffer_ + 1;

    hC2S_DataAvailableEvent_ = OpenEventA(EVENT_ALL_ACCESS, FALSE, c2s_event_name.c_str());
    hS2C_DataAvailableEvent_ = OpenEventA(EVENT_ALL_ACCESS, FALSE, s2c_event_name.c_str());

    if (!hC2S_DataAvailableEvent_ || !hS2C_DataAvailableEvent_) {
        Shutdown();
        std::cerr << "Failed to open synchronization objects." << std::endl;
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "Connected to shared memory server." << std::endl;
    }
    listen_thread_ = std::make_unique<std::thread>(&RpcSystem::ListenLoop);
    return true;
}

namespace {
    // Helper to write data to the circular buffer, handling wrap-around.
    void write_data_to_buffer(ignition::rpc::CircularBuffer& cb, size_t& tail, const void* src, size_t count) {
        const char* src_bytes = static_cast<const char*>(src);
        size_t first_chunk_size = std::min(count, ignition::rpc::SHM_BUFFER_SIZE - tail);
        memcpy(&cb.buffer[tail], src_bytes, first_chunk_size);

        if (first_chunk_size < count) {
            memcpy(&cb.buffer[0], src_bytes + first_chunk_size, count - first_chunk_size);
        }
        tail = (tail + count) % ignition::rpc::SHM_BUFFER_SIZE;
    }

    // Helper to read data from the circular buffer, handling wrap-around.
    void read_data_from_buffer(ignition::rpc::CircularBuffer& cb, size_t& head, void* dest, size_t count) {
        char* dest_bytes = static_cast<char*>(dest);
        size_t first_chunk_size = std::min(count, ignition::rpc::SHM_BUFFER_SIZE - head);
        memcpy(dest_bytes, &cb.buffer[head], first_chunk_size);

        if (first_chunk_size < count) {
            memcpy(dest_bytes + first_chunk_size, &cb.buffer[0], count - first_chunk_size);
        }
        head = (head + count) % ignition::rpc::SHM_BUFFER_SIZE;
    }

    bool WriteToCircularBuffer(ignition::rpc::CircularBuffer& cb, const std::vector<char>& data) {
        // Acquire spin-lock
        while (cb.lock.test_and_set(std::memory_order_acquire)) {}

        const size_t message_size = data.size();
        const size_t total_size = sizeof(size_t) + message_size;

        size_t free_space;
        if (cb.head <= cb.tail) {
            free_space = ignition::rpc::SHM_BUFFER_SIZE - (cb.tail - cb.head);
        } else {
            free_space = cb.head - cb.tail;
        }

        if (total_size >= free_space) {
            cb.lock.clear(std::memory_order_release);
            return false; // Not enough space
        }

        // Write message size
        size_t current_tail = cb.tail;
        write_data_to_buffer(cb, current_tail, &message_size, sizeof(size_t));

        // Write message data (potentially wrapping around)
        write_data_to_buffer(cb, current_tail, data.data(), message_size);

        cb.tail = current_tail;

        cb.lock.clear(std::memory_order_release);
        return true;
    }

    bool ReadFromCircularBuffer(ignition::rpc::CircularBuffer& cb, std::vector<char>& data) {
        // Acquire spin-lock
        while (cb.lock.test_and_set(std::memory_order_acquire)) {}

        bool hasRead = false;

        do {
            try
            {
                if (cb.head == cb.tail) {
                    break;
                }

                // Read message size
                size_t message_size;
                size_t current_head = cb.head;
                read_data_from_buffer(cb, current_head, &message_size, sizeof(size_t));

                data.resize(message_size);

                // Read message data (potentially wrapping around)
                read_data_from_buffer(cb, current_head, data.data(), message_size);

                cb.head = current_head;

                hasRead = true;
            }
            catch (...) {
                std::cerr << "Exception in ReadFromCircularBuffer" << std::endl;
                break;
            }
        } while (false);

        cb.lock.clear(std::memory_order_release);
        return hasRead;
    }
}

void RpcSystem::ListenLoop() {
    HANDLE hDataAvailable = is_server_ ? hC2S_DataAvailableEvent_ : hS2C_DataAvailableEvent_;
    ignition::rpc::CircularBuffer* pReadBuffer = is_server_ ? pC2S_Buffer_ : pS2C_Buffer_;
    
    while (running_) {
        DWORD waitResult = WaitForSingleObject(hDataAvailable, INFINITE);

        if (!running_) {
            break;
        }

        if (waitResult == WAIT_TIMEOUT) {
            continue;
        }

        if (waitResult == WAIT_OBJECT_0) {
            std::vector<char> message;
            while (ReadFromCircularBuffer(*pReadBuffer, message)) {
                ProcessMessage(message);
            }

        } else {
            // Error or abandoned wait
            std::cerr << "Listener wait failed. Error: " << GetLastError() << std::endl;
            running_ = false;
            break;
        }
    }
}

void RpcSystem::ProcessMessage(const std::vector<char>& buffer) {
    if (buffer.empty()) return;

    const char* ptr = buffer.data();
    const char* end_ptr = buffer.data() + buffer.size();
    char msg_type = *ptr++;
    
    if (msg_type == 'C') { // Call
        uint32_t callId;
        memcpy(&callId, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        uint32_t name_len;
        memcpy(&name_len, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        std::string func_name(ptr, name_len);
        ptr += name_len;

        uint32_t arg_count;
        memcpy(&arg_count, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        std::vector<RpcValue> args;
        for (uint32_t i = 0; i < arg_count; ++i) {
            args.push_back(Serializer::deserialize(ptr, end_ptr));
        }

        // Enqueue the task to be executed by the thread pool
        EnqueueTask([func_name, args, callId] {
            RpcValue return_val;
            try {
                RpcFunction func;
                {
                    std::lock_guard<std::mutex> lock(registry_mutex_);
                    if (!function_registry_.count(func_name)) {
                        throw std::runtime_error("Function not found: " + func_name);
                    }
                    func = function_registry_.at(func_name);
                }
                if (func) {
                    return_val = func(args);
                } else {
                    throw std::runtime_error("Function not found: " + func_name);
                }
            } catch (const std::exception& e) {
                {
                    std::lock_guard<std::mutex> lock(cout_mutex_);
                    std::cerr << "RPC Error on call to '" << func_name << "': " << e.what() << std::endl;
                }
                // return_val is default-constructed (T_NULL)
            }

            std::vector<char> return_buffer;
            char return_msg_type = 'R'; // 'R' for Return
            return_buffer.push_back(return_msg_type);
            return_buffer.insert(return_buffer.end(), reinterpret_cast<const char*>(&callId), reinterpret_cast<const char*>(&callId) + sizeof(callId));
            Serializer::serialize(return_buffer, return_val);

            SendRPCMessage(return_buffer);
        });

    } else if (msg_type == 'R') { // Return
        uint32_t callId;
        memcpy(&callId, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        RpcValue val = Serializer::deserialize(ptr, end_ptr);
        
        std::shared_ptr<PendingCall> pendingCall;
        {
            std::lock_guard<std::mutex> lock(pending_calls_mutex_);
            if (pending_calls_.count(callId)) {
                pendingCall = pending_calls_[callId];
                pending_calls_.erase(callId);
            }
        }
        
        if (pendingCall) {
            std::lock_guard<std::mutex> lock(pendingCall->mtx);
            pendingCall->returnValue = val;
            pendingCall->completed = true;
            pendingCall->cv.notify_one();
        }
        else
        {
            {
                std::lock_guard<std::mutex> lock(cout_mutex_);
                std::cerr << "Received return for unknown call ID: " << callId << std::endl;
            }
        }
    }
    else {
        {
            std::lock_guard<std::mutex> lock(cout_mutex_);
            std::cerr << "Unknown message type received: " << msg_type << std::endl;
        }
    }
}

void RpcSystem::SendRPCMessage(const std::vector<char>& buffer) {
    if (!running_ || !IsConnected() || buffer.empty()) {
        return;
    }

    ignition::rpc::CircularBuffer* pWriteBuffer = is_server_ ? pS2C_Buffer_ : pC2S_Buffer_;
    HANDLE hDataAvailable = is_server_ ? hS2C_DataAvailableEvent_ : hC2S_DataAvailableEvent_;

    bool success = WriteToCircularBuffer(*pWriteBuffer, buffer);

    if (success) {
        SetEvent(hDataAvailable);
    } else {
        std::cerr << "Failed to write to circular buffer (full)." << std::endl;
    }
}

// --- Object Management ---
RpcObjectId RpcSystem::GenerateObjectId() {
    // Simple increment for now. A real system might want UUIDs.
    // If this is a server, generate from the upper half of the range.
    // If a client, from the lower half. This prevents collisions.
    static bool is_server_init = is_server_;
    static std::atomic<uint64_t> local_next_id{ is_server_init ? (1ULL << 32) : 1 };
    return local_next_id++;
}

void RpcSystem::RegisterLocalObject(RpcObject* obj) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    local_objects_[obj->GetId()] = obj;
}

void RpcSystem::UnregisterLocalObject(RpcObjectId id) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    local_objects_.erase(id);
}

RpcObject* RpcSystem::FindOrCreateProxy(RpcObjectId id, const std::string& className) {
    std::lock_guard<std::mutex> lock(object_mutex_);

    // First, check if it's actually a local object being passed back to us
    if (local_objects_.count(id)) {
        return local_objects_.at(id);
    }
    
    // Check if a proxy already exists
    if (remote_proxies_.count(id)) {
        return remote_proxies_.at(id).get();
    }
    
    // Create a new proxy using the registered factory
    if (object_factories_.count(className)) {
        auto new_proxy = object_factories_.at(className)(id);
        RpcObject* proxy_ptr = new_proxy.get();
        remote_proxies_[id] = std::move(new_proxy);
        return proxy_ptr;
    }

    throw std::runtime_error("Cannot create proxy: Class '" + className + "' is not registered.");
}
