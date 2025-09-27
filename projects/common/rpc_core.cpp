#include "rpc_core.h"
#include "mapped_event_circular_buffer_c.h"
#include <stdexcept>
#include <algorithm>
#include <cassert>
#include <vector>

// --- RpcValue Implementation ---
RpcValue::RpcValue(RpcObject* v) : type_(T_OBJECT_REF) {
    assert(v != nullptr && "Cannot construct RpcValue with a null RpcObject*");
    obj_ref_.id = v->GetId();
    obj_ref_.class_name = &v->GetRpcClassName();
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
    return RpcSystem::GetInstance()._FindOrCreateProxy(obj_ref_.id, *obj_ref_.class_name);
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
RpcSystem& RpcSystem::GetInstance() {
    static RpcSystem instance;
    return instance;
}

RpcSystem::RpcSystem() = default;

RpcSystem::~RpcSystem() {
    _Shutdown();
}

void RpcSystem::_Initialize(const std::string& pipeName) {
    pipe_name_ = pipeName;
    next_object_id_ = 1;
    next_call_id_ = 1;
    running_ = true;

    _InitializeThreadPool(5);
}

void RpcSystem::_Shutdown() {
    if (!running_) {
        return;
    }
    running_ = false;

    _ShutdownThreadPool();

    // The listen thread might be waiting on the circular buffer.
    // A robust shutdown would signal the event or close the handle to wake it up.
    if (listen_thread_ && listen_thread_->joinable()) {
        listen_thread_->join();
    }

    mapped_event_circular_buffer_close_shm(pC2S_Buffer_);
    mapped_event_circular_buffer_close_shm(pS2C_Buffer_);
    pC2S_Buffer_ = nullptr;
    pS2C_Buffer_ = nullptr;

    // Clear out proxies to break potential circular references
    remote_proxies_.clear();
}

void RpcSystem::_InitializeThreadPool(size_t num_threads) {
    stop_thread_pool_ = false;
    for (size_t i = 0; i < num_threads; ++i) {
        worker_threads_.emplace_back([this] {
            while (true) {
                std::function<void()> task;
                {
                    std::unique_lock<std::mutex> lock(thread_pool_mutex_);
                    thread_pool_cv_.wait(lock, [this] { return stop_thread_pool_ || !tasks_.empty(); });
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

void RpcSystem::_ShutdownThreadPool() {
    {
        std::unique_lock<std::mutex> lock(thread_pool_mutex_);
        stop_thread_pool_ = true;
    }

    thread_pool_cv_.notify_all();
}

bool RpcSystem::_IsConnected() {
    return pC2S_Buffer_ != nullptr && pS2C_Buffer_ != nullptr;
}

void RpcSystem::_RegisterFunction(const std::string& name, RpcFunction func) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    function_registry_[name] = func;
}

void RpcSystem::_UnregisterFunction(const std::string& name) {
     std::lock_guard<std::mutex> lock(registry_mutex_);
     function_registry_.erase(name);
}

void RpcSystem::_StartServer() {
    is_server_ = true;
    std::string c2s_name = pipe_name_ + "_c2s";
    std::string s2c_name = pipe_name_ + "_s2c";

    pC2S_Buffer_ = mapped_event_circular_buffer_create_shm(c2s_name.c_str(), SHM_BUFFER_SIZE);
    pS2C_Buffer_ = mapped_event_circular_buffer_create_shm(s2c_name.c_str(), SHM_BUFFER_SIZE);

    if (!pC2S_Buffer_ || !pS2C_Buffer_) {
        _Shutdown();
        std::cerr << "Failed to create shared memory buffers." << std::endl;
        throw std::runtime_error("Failed to create shared memory buffers.");
    }

    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "Shared memory server running. Waiting for client..." << std::endl;
    }

    listen_thread_ = std::make_unique<std::thread>(&RpcSystem::ListenLoop, this);
}

bool RpcSystem::_ConnectToServer() {
    is_server_ = false;
    std::string c2s_name = pipe_name_ + "_c2s";
    std::string s2c_name = pipe_name_ + "_s2c";

    pC2S_Buffer_ = mapped_event_circular_buffer_open_shm(c2s_name.c_str(), SHM_BUFFER_SIZE);
    pS2C_Buffer_ = mapped_event_circular_buffer_open_shm(s2c_name.c_str(), SHM_BUFFER_SIZE);

    if (!pC2S_Buffer_ || !pS2C_Buffer_) {
        std::cerr << "Failed to open shared memory buffers." << std::endl;
        _Shutdown();
        return false;
    }

    {
        std::lock_guard<std::mutex> lock(cout_mutex_);
        std::cout << "Connected to shared memory server." << std::endl;
    }

    listen_thread_ = std::make_unique<std::thread>(&RpcSystem::ListenLoop, this);
    return true;
}

void RpcSystem::ListenLoop() {
    CircularBuffer* pReadBuffer = is_server_ ? pC2S_Buffer_ : pS2C_Buffer_;

    static char message[SHM_BUFFER_SIZE];
    
    while (running_) {
        mapped_event_circular_buffer_wait_for_data(pReadBuffer);

        if (!running_) {
            break;
        }

        size_t message_size = SHM_BUFFER_SIZE;

        while (mapped_event_circular_buffer_read(pReadBuffer, message, &message_size)) {
            ProcessMessage(std::vector<char>(message, message + message_size));
            
            message_size = SHM_BUFFER_SIZE;
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

        //std::cout << "Received call for function: " << func_name << " with " << arg_count << " args." << std::endl;

        // Enqueue the task to be executed by the thread pool
        EnqueueTask([this, func_name, args, callId] {
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

    CircularBuffer* pWriteBuffer = is_server_ ? pS2C_Buffer_ : pC2S_Buffer_;

    bool success = mapped_event_circular_buffer_write(pWriteBuffer, buffer.data(), buffer.size());

    if (!success) {
        std::cerr << "Failed to write to circular buffer (full)." << std::endl;
    }
 }

// --- Object Management ---
RpcObjectId RpcSystem::_GenerateObjectId() {
    // Simple increment for now. A real system might want UUIDs.
    // If this is a server, generate from the upper half of the range.
    // If a client, from the lower half. This prevents collisions.
    static bool is_server_init = is_server_;
    static std::atomic<uint64_t> local_next_id{ is_server_init ? (1ULL << 32) : 1 };
    return local_next_id++;
}

void RpcSystem::_RegisterLocalObject(RpcObject* obj) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    local_objects_[obj->GetId()] = obj;
}

void RpcSystem::_UnregisterLocalObject(RpcObjectId id) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    local_objects_.erase(id);
}

RpcObject* RpcSystem::_FindOrCreateProxy(RpcObjectId id, const std::string& className) {
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
