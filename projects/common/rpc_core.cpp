#include "rpc_core.h"
#include "mapped_event_circular_buffer_c.h"

#include <algorithm>
#include <cassert>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <pthread.h>
#endif

// --- RpcValue Implementation ---
RpcValue::RpcValue(RpcObject* v) : type_(T_OBJECT_REF) {
    assert(v != nullptr && "Cannot construct RpcValue with a null RpcObject*");
    obj_ref_.id = v->GetId();    
    obj_ref_.class_id = v->GetRpcClassId();
}

RpcValue::RpcValue(const RpcValue& other) : type_(other.type_) {
    switch (type_) {
        case T_INT:        int_val_        = other.int_val_;        break;
        case T_FLOAT:      float_val_      = other.float_val_;      break;
        case T_DOUBLE:     double_val_     = other.double_val_;     break;
        case T_UINT64:     uint64_val_     = other.uint64_val_;     break;
        case T_OBJECT_REF: obj_ref_        = other.obj_ref_;        break;
        case T_BYTE_ARRAY: byte_array_val_ = other.byte_array_val_; break;
        case T_NULL:       /* Nothing to copy */                    break;
    }
}

RpcValue& RpcValue::operator=(const RpcValue& other) {
    RpcValue(other).swap(*this);
    return *this;
}

void RpcValue::swap(RpcValue& other) noexcept {
    using std::swap;
    swap(type_, other.type_);
    swap(byte_array_val_, other.byte_array_val_); // Swaps both string and pointer data

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
uint64_t RpcValue::asUint64() const {
    if (!isUint64())
    {
        throw std::runtime_error("RpcValue is not a uint64_t.");
    }
    return uint64_val_;
}
const std::vector<char>& RpcValue::asByteArray() const {
    if (!isByteArray()) throw std::runtime_error("RpcValue is not a byte array (vector).");
    return byte_array_val_;
}
RpcObject* RpcValue::asObject() const {
    if (!isObject()) throw std::runtime_error("RpcValue is not an object reference.");
    return RpcSystem::GetInstance()._FindOrCreateProxy(obj_ref_.id, obj_ref_.class_id);
}

std::string RpcValue::asString() const {
    if (!isByteArray()) throw std::runtime_error("RpcValue is not a byte array (string).");
    return std::string(byte_array_val_.begin(), byte_array_val_.end());
}

// --- RpcObject Implementation ---
void RpcObject::RegisterFunction(RpcFunctionEnum funcId, RpcFunction func) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    function_registry_[funcId] = func;
}

void RpcObject::UnregisterFunction(RpcFunctionEnum funcId) {
     std::lock_guard<std::mutex> lock(registry_mutex_);
     function_registry_.erase(funcId);
}

RpcFunction RpcObject::FindFunction(RpcFunctionEnum funcId) {
    std::lock_guard<std::mutex> lock(registry_mutex_);
    auto it = function_registry_.find(funcId);
    return (it == function_registry_.end()) ? nullptr : it->second;
}

// --- RpcSerializer Implementation ---
void RpcSerializer::Serialize(std::vector<char>& buffer, const RpcValue& val) {
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
        case RpcValue::T_BYTE_ARRAY: {
            const auto& data_vec = val.byte_array_val_;
            uint32_t len = data_vec.size();
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&len), reinterpret_cast<const char*>(&len) + sizeof(uint32_t));
            buffer.insert(buffer.end(), data_vec.begin(), data_vec.end());
            break;
        }
        case RpcValue::T_OBJECT_REF: {
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val.obj_ref_.id), reinterpret_cast<const char*>(&val.obj_ref_.id) + sizeof(RpcObjectId));
            buffer.insert(buffer.end(), reinterpret_cast<const char*>(&val.obj_ref_.class_id), reinterpret_cast<const char*>(&val.obj_ref_.class_id) + sizeof(RpcClassEnum));
            break;            
        }
        case RpcValue::T_NULL:
            break;
    }
}

RpcValue RpcSerializer::Deserialize(const char*& bufferPtr, const char* bufferEnd) {
    if (bufferPtr >= bufferEnd) throw std::runtime_error("Deserialization buffer underflow.");
    
    RpcValue::Type type = static_cast<RpcValue::Type>(*bufferPtr++);
    
    switch (type) {
        case RpcValue::T_INT: {
            int val;
            memcpy(&val, bufferPtr, sizeof(int));
            bufferPtr += sizeof(int);
            return RpcValue(val);
        }
        case RpcValue::T_FLOAT: {
            float val;
            memcpy(&val, bufferPtr, sizeof(float));
            bufferPtr += sizeof(float);
            return RpcValue(val);
        }
        case RpcValue::T_DOUBLE: {
            double val;
            memcpy(&val, bufferPtr, sizeof(double));
            bufferPtr += sizeof(double);
            return RpcValue(val);
        }
        case RpcValue::T_UINT64: {
            uint64_t val;
            memcpy(&val, bufferPtr, sizeof(uint64_t));
            bufferPtr += sizeof(uint64_t);
            return RpcValue(val);
        }
        case RpcValue::T_BYTE_ARRAY: {
            uint32_t len;
            memcpy(&len, bufferPtr, sizeof(uint32_t));
            bufferPtr += sizeof(uint32_t);
            RpcValue v(bufferPtr, len);
            bufferPtr += len;
            return v;
        }
        case RpcValue::T_OBJECT_REF: {
            RpcObjectId id;
            memcpy(&id, bufferPtr, sizeof(RpcObjectId));
            bufferPtr += sizeof(RpcObjectId);

            RpcClassEnum class_id;
            memcpy(&class_id, bufferPtr, sizeof(RpcClassEnum));
            bufferPtr += sizeof(RpcClassEnum);
            
            RpcObject* obj = RpcSystem::GetInstance()._FindOrCreateProxy(id, class_id);
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

void RpcSystem::_Initialize(const std::string& ipcName) {
    pipe_name_ = ipcName;
    next_object_id_ = 1;
    next_call_id_ = 1;
    running_ = true;

    // Replace '.' with '_'
    std::replace(pipe_name_.begin(), pipe_name_.end(), '.', '_');

    _InitializeThreadPool(5);
}

void RpcSystem::_Shutdown() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }
    
    // Listen thread calls mapped_event_circular_buffer_wait_for_data, which is set to timeout after 1 second.
    // It will exit the loop since running_ is now false.
    if (listen_thread_ && listen_thread_->joinable()) {
        listen_thread_->join();
    }

    // Complete all pending calls
    {
        std::lock_guard<std::mutex> lock(pending_calls_mutex_);
        for (auto const& [callId, pendingCall] : pending_calls_) {
            std::lock_guard<std::mutex> call_lock(pendingCall->mtx);
            pendingCall->completed = true; // Mark as completed to unblock waiters
            pendingCall->cv.notify_one();
        }
        pending_calls_.clear();
    }

    _ShutdownThreadPool();

    // Clean up shared memory resources
    mapped_event_circular_buffer_close_shm(pC2S_Buffer_);
    mapped_event_circular_buffer_close_shm(pS2C_Buffer_);

    mapped_event_circular_buffer_destroy(pC2S_Buffer_);
    mapped_event_circular_buffer_destroy(pS2C_Buffer_);
    pC2S_Buffer_ = nullptr;
    pS2C_Buffer_ = nullptr;

    // Clear out proxies to break potential circular references
    remote_proxies_.clear();
}

void RpcSystem::_InitializeThreadPool(size_t numThreads) {
    stop_thread_pool_ = false;
    for (size_t i = 0; i < numThreads; ++i) {
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

    for (std::thread& worker : worker_threads_) {
        if (worker.joinable()) {
            worker.join();
        }
    }
    worker_threads_.clear();
}

bool RpcSystem::_IsConnected() {
    return pC2S_Buffer_ != nullptr && pS2C_Buffer_ != nullptr;
}

void RpcSystem::_CreateIPC() {
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

    listen_thread_ = std::make_unique<std::thread>(&RpcSystem::ListenLoop, this);
}

bool RpcSystem::_ConnectToExistingIPC() {
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

    listen_thread_ = std::make_unique<std::thread>(&RpcSystem::ListenLoop, this);
    return true;
}

void RpcSystem::ListenLoop() {
    CircularBuffer* pReadBuffer = is_server_ ? pC2S_Buffer_ : pS2C_Buffer_;

    static char message[SHM_BUFFER_SIZE];
    
    while (running_) {
        mapped_event_circular_buffer_wait_for_data(pReadBuffer, 1000);

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
    const char* endPtr = buffer.data() + buffer.size();
    char msgType = *ptr++;
    
    if (msgType == 'P') { // Ping
        std::vector<char> ackBuffer;
        ackBuffer.push_back('A'); // 'A' for Ack
        ackBuffer.insert(ackBuffer.end(), ptr, endPtr);
        SendRPCMessage(ackBuffer);
        return;
    }
    if (msgType == 'C') { // Call
        uint32_t callId;
        memcpy(&callId, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        RpcObjectId objId;
        memcpy(&objId, ptr, sizeof(RpcObjectId));
        ptr += sizeof(RpcObjectId);

        RpcFunctionEnum funcId;
        memcpy(&funcId, ptr, sizeof(RpcFunctionEnum));
        ptr += sizeof(RpcFunctionEnum);

        uint32_t argCount;
        memcpy(&argCount, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);
        
        std::vector<RpcValue> args;
        for (uint32_t i = 0; i < argCount; ++i) {
            args.push_back(RpcSerializer::Deserialize(ptr, endPtr));
        }

        // Enqueue the task to be executed by the thread pool
        EnqueueTask([this, objId, funcId, args, callId] {
            RpcValue returnVal;
            try {
                if (objId == 0) { // Static function call
                    RpcFunction func = _FindFunction(funcId);
                    if (func) {
                        returnVal = func(args);
                    } else {
                        throw std::runtime_error("Static function ID not found: " + std::to_string(funcId));
                    }
                } else {
                    RpcObject* target_obj = _GetLocalObject(objId);
                    if (!target_obj) {
                        throw std::runtime_error("Target object not found: " + std::to_string(objId));
                    }
                    RpcFunction func = target_obj->FindFunction(funcId);
                    if (func) {
                        returnVal = func(args);
                    } else {
                         throw std::runtime_error("Method ID " + std::to_string(funcId) + " not found on object " + std::to_string(objId));
                    }
                }
            } catch (const std::exception& e) {
                std::cerr << "RPC Error on call to '" << funcId << "': " << e.what() << std::endl;
                // returnVal is default-constructed (T_NULL)
            }

            std::vector<char> returnBuffer;
            char returnMsgType = 'R'; // 'R' for Return
            returnBuffer.push_back(returnMsgType);
            returnBuffer.insert(returnBuffer.end(), reinterpret_cast<const char*>(&callId), reinterpret_cast<const char*>(&callId) + sizeof(callId));
            RpcSerializer::Serialize(returnBuffer, returnVal);

            SendRPCMessage(returnBuffer);
        });

    } else if (msgType == 'A') { // Ack
        uint32_t pingId;
        memcpy(&pingId, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        std::shared_ptr<PendingCall> pendingCall;
        {
            std::lock_guard<std::mutex> lock(pending_calls_mutex_);
            if (pending_calls_.count(pingId)) {
                pendingCall = pending_calls_[pingId];
                pendingCall->returnValue = RpcValue(true);
                pendingCall->completed = true;
                pendingCall->cv.notify_one();
            }
        }

    } else if (msgType == 'R') { // Return
        uint32_t callId;
        memcpy(&callId, ptr, sizeof(uint32_t));
        ptr += sizeof(uint32_t);

        RpcValue val = RpcSerializer::Deserialize(ptr, endPtr);
        
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
            std::cerr << "Received return for unknown call ID: " << callId << std::endl;
        }
    }
    else {
        std::cerr << "Unknown message type received: " << msgType << std::endl;
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
    static bool isServerInit = is_server_;
    static std::atomic<uint64_t> localNextId{ isServerInit ? (1ULL << 32) : 1 };
    return localNextId++;
}

void RpcSystem::_RegisterLocalObject(RpcObject* obj) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    local_objects_[obj->GetId()] = obj;
}

void RpcSystem::_UnregisterLocalObject(RpcObjectId id) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    local_objects_.erase(id);
}

void RpcSystem::_RegisterFunction(RpcFunctionEnum funcId, RpcFunction func) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    static_function_registry_[funcId] = func;
}

void RpcSystem::_UnregisterFunction(RpcFunctionEnum funcId) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    static_function_registry_.erase(funcId);
}

RpcFunction RpcSystem::_FindFunction(RpcFunctionEnum funcId) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    auto it = static_function_registry_.find(funcId);
    return (it == static_function_registry_.end()) ? nullptr : it->second;
}

RpcObject* RpcSystem::_GetLocalObject(RpcObjectId id) {
    std::lock_guard<std::mutex> lock(object_mutex_);
    auto it = local_objects_.find(id);
    return (it == local_objects_.end()) ? nullptr : it->second;
}

RpcObject* RpcSystem::_FindOrCreateProxy(RpcObjectId id, RpcClassEnum classId) {
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
    if (object_factories_.count(classId)) {
        auto new_proxy = object_factories_.at(classId)(id);
        RpcObject* proxy_ptr = new_proxy.get();
        remote_proxies_[id] = std::move(new_proxy);
        return proxy_ptr;
    }

    throw std::runtime_error("Cannot create proxy: Class ID '" + std::to_string(classId) + "' is not registered.");
}

RpcValue RpcSystem::InternalCall(RpcObjectId objId, RpcFunctionEnum funcId, const std::vector<RpcValue>& args) {
    if (!_IsConnected()) {
        throw std::runtime_error("RPC system is not connected.");
    }

    // Stop future calls by exiting this thread. We will not have any safe values to return at this point.
    if (!running_) {
#ifdef _WIN32
        ExitThread(0);
#else
        pthread_exit(nullptr);
#endif
    }

    uint32_t callId;
    auto pendingCall = std::make_shared<PendingCall>();
    {
        std::lock_guard<std::mutex> lock(pending_calls_mutex_);
        callId = next_call_id_++;
        pending_calls_[callId] = pendingCall;
    }

    std::vector<char> buffer;
    char msgType = 'C';
    buffer.push_back(msgType);
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&callId), reinterpret_cast<const char*>(&callId) + sizeof(callId));
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&objId), reinterpret_cast<const char*>(&objId) + sizeof(objId));
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&funcId), reinterpret_cast<const char*>(&funcId) + sizeof(funcId));

    uint32_t argCount = args.size();
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&argCount), reinterpret_cast<const char*>(&argCount) + sizeof(argCount));
    for (const auto& arg : args) {
        RpcSerializer::Serialize(buffer, arg);
    }

    SendRPCMessage(buffer);
    // Wait for the return value
    std::unique_lock<std::mutex> lock(pendingCall->mtx);
    if (!pendingCall->cv.wait_for(lock, std::chrono::seconds(60), [&]{ return pendingCall->completed; })) {
        {
            std::lock_guard<std::mutex> pc_lock(pending_calls_mutex_);
            pending_calls_.erase(callId);
        }
        throw std::runtime_error("RPC call timed out for object " + std::to_string(objId) + " function " + std::to_string(funcId));
    }

    return pendingCall->returnValue;
}

bool RpcSystem::_IsAlive() {
    if (!_IsConnected()) {
        return false;
    }

    uint32_t callId;
    auto pendingCall = std::make_shared<PendingCall>();
    {
        std::lock_guard<std::mutex> lock(pending_calls_mutex_);
        callId = next_call_id_++;
        pending_calls_[callId] = pendingCall;
    }

    std::vector<char> buffer;
    char msgType = 'P'; // 'P' for Ping
    buffer.push_back(msgType);
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&callId), reinterpret_cast<const char*>(&callId) + sizeof(callId));

    SendRPCMessage(buffer);

    std::unique_lock<std::mutex> lock(pendingCall->mtx);
    if (!pendingCall->cv.wait_for(lock, std::chrono::milliseconds(200), [&]{ return pendingCall->completed; })) {
        {
            std::lock_guard<std::mutex> pc_lock(pending_calls_mutex_);
            pending_calls_.erase(callId);
        }
        return false; // Timed out
    }
    return pendingCall->returnValue.asInt();
}