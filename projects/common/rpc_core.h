#ifndef RPC_CORE_H
#define RPC_CORE_H

#define NOMINMAX

#include <windows.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <iostream>
#include <sstream>
#include <atomic>
#include <queue>
#include <utility>

namespace ignition
{
    namespace rpc
    {
        constexpr size_t SHM_BUFFER_SIZE = 1024 * 128; // 128 KB

        struct CircularBuffer {
            std::atomic_flag lock = ATOMIC_FLAG_INIT;
            std::atomic<size_t> head;
            std::atomic<size_t> tail;
            char buffer[SHM_BUFFER_SIZE];
        };
    }
}

// Forward declarations
class RpcSystem; // Now a static class
class RpcObject;

// Unique identifier for an RPC-enabled object instance
using RpcObjectId = uint64_t;

// --- Argument Serialization ---
// A type-erased container for arguments and return values.
class RpcValue {
public:
    RpcValue() : type_(T_NULL) {}
    explicit RpcValue(int v) : type_(T_INT), int_val_(v) {}
    explicit RpcValue(float v) : type_(T_FLOAT), float_val_(v) {}
    explicit RpcValue(const std::string& v) : type_(T_STRING), str_val_(v) {}
    explicit RpcValue(uint64_t v) : type_(T_UINT64), uint64_val_(v) {}
    RpcValue(const char* data, size_t len) : type_(T_POINTER), ptr_val_(data, data + len) {}
    RpcValue(RpcObject* v);

    // Copy constructor and assignment
    RpcValue(const RpcValue& other);
    RpcValue& operator=(const RpcValue& other);
    void swap(RpcValue& other) noexcept;
    ~RpcValue();

    // Type checking
    bool isInt() const { return type_ == T_INT; }
    bool isFloat() const { return type_ == T_FLOAT; }
    bool isString() const { return type_ == T_STRING; }
    bool isPointer() const { return type_ == T_POINTER; }
    bool isUint64() const { return type_ == T_UINT64; }
    bool isObject() const { return type_ == T_OBJECT_REF; }

    // Value accessors
    int asInt() const;
    float asFloat() const;
    std::string asString() const;
    uint64_t asUint64() const;
    std::pair<const char*, size_t> asPointer() const;
    RpcObject* asObject() const; // RpcSystem is now static

private:
    friend class Serializer;
    enum Type { T_NULL, T_INT, T_FLOAT, T_STRING, T_POINTER, T_OBJECT_REF, T_UINT64 };
    Type type_;

    // Holds info to look up a remote object
    struct ObjectRef {
        RpcObjectId id;
        const std::string* class_name; // Used to create proxies
    };

    union {
        int int_val_ = 0;
        float float_val_;
        uint64_t uint64_val_;
        ObjectRef obj_ref_;
    };
    // Value-based storage for variable-length data
    std::string str_val_{};
    std::vector<char> ptr_val_{};
};

// --- RPC Object Base Class ---
// Inherit from this class to make it usable over RPC.
class RpcObject {
public:
    // Constructor for a new local object. It will be registered with the system.
    RpcObject();
    // Constructor for a proxy to a remote object. Use this in derived classes.
    RpcObject(RpcObjectId id);

    virtual ~RpcObject();

    RpcObjectId GetId() const { return object_id_; }
    bool IsProxy() const { return is_proxy_; }

    // Must be implemented by derived classes to identify the type for proxy creation.
    virtual const std::string& GetRpcClassName() const = 0;

protected:
    RpcObjectId object_id_;
    const bool is_proxy_;
};


// --- RPC System (Static Class) ---
class RpcSystem {
public:
    using RpcFunction = std::function<RpcValue(const std::vector<RpcValue>&)>;

    // Deleted constructor to enforce static-only usage
    RpcSystem() = delete;

    // Must be called once at the start of the application
    static void Initialize(const std::string& pipeName);
    static void InitializeThreadPool(size_t num_threads);

    static void RegisterFunction(const std::string& name, RpcFunction func);
    static void UnregisterFunction(const std::string& name);

    // Register a class type and its proxy factory.
    template<typename T>
    static void RegisterClass();

    // Call a remote standalone function.
    template<typename... Args>
    static RpcValue Call(const std::string& funcName, Args... args);

    // Call a remote method on an object.
    template<typename... Args>
    static RpcValue CallMethod(RpcObjectId objId, const std::string& methodName, Args... args);

    static void StartServer();
    static bool ConnectToServer();

    static void Shutdown();
    static void ShutdownThreadPool();
    static bool IsConnected();

    // --- Object Management (public for RpcObject and RpcValue) ---
    static RpcObjectId GenerateObjectId();
    static void RegisterLocalObject(RpcObject* obj);
    static void UnregisterLocalObject(RpcObjectId id);
    static RpcObject* FindOrCreateProxy(RpcObjectId id, const std::string& className);

private:
    struct PendingCall {
        std::condition_variable cv;
        std::mutex mtx;
        RpcValue returnValue;
        bool completed = false;
    };
    
    using ObjectFactory = std::function<std::unique_ptr<RpcObject>(RpcObjectId)>;

    static void ListenLoop();
    static void ProcessMessage(const std::vector<char>& buffer);    
    static void SendRPCMessage(const std::vector<char>& buffer);
    template<class F, class... Args>
    static void EnqueueTask(F&& f, Args&&... args);

    static RpcValue InternalCall(RpcObjectId objId, const std::string& funcName, const std::vector<RpcValue>& args);
    
    static std::string pipe_name_;
    static bool is_server_;
    static std::unique_ptr<std::thread> listen_thread_;
    static bool running_;

    // --- Shared Memory & Events ---
    static HANDLE hMapFile_;
    static void* pSharedMem_;

    static ignition::rpc::CircularBuffer* pC2S_Buffer_; // Client-to-Server
    static ignition::rpc::CircularBuffer* pS2C_Buffer_; // Server-to-Client

    static HANDLE hC2S_DataAvailableEvent_;
    static HANDLE hS2C_DataAvailableEvent_;
    static HANDLE hWriteMutex_; // Single mutex for writing to either buffer

    // --- Function and Object Registries ---
    static std::map<std::string, RpcFunction> function_registry_;
    static std::mutex registry_mutex_;
    
    static std::atomic<RpcObjectId> next_object_id_;
    static std::map<RpcObjectId, RpcObject*> local_objects_; // Real objects this process owns
    static std::map<RpcObjectId, std::unique_ptr<RpcObject>> remote_proxies_; // Proxies to remote objects
    static std::map<std::string, ObjectFactory> object_factories_;
    static std::mutex object_mutex_;

    // --- Synchronization for Calls ---
    static std::map<uint32_t, std::shared_ptr<PendingCall>> pending_calls_;
    static std::mutex pending_calls_mutex_;
    static std::atomic<uint32_t> next_call_id_;

    // --- Synchronous Sender ---
    static std::mutex send_mutex_;
    // --- Thread Pool for handling calls ---
    static std::vector<std::thread> worker_threads_;
    static std::queue<std::function<void()>> tasks_;
    static std::mutex thread_pool_mutex_;
    static std::condition_variable thread_pool_cv_;
    static bool stop_thread_pool_;
    static std::mutex cout_mutex_;
};


// Simple serializer
class Serializer {
public:
    static void serialize(std::vector<char>& buffer, const RpcValue& val);
    static RpcValue deserialize(const char*& buffer_ptr, const char* buffer_end);
};


// --- Template & Inline Implementations ---

template<typename T>
void RpcSystem::RegisterClass() {
    T dummy_for_name(1); // Create a dummy proxy to get the class name
    const std::string& className = dummy_for_name.GetRpcClassName();
    
    std::lock_guard<std::mutex> lock(object_mutex_);
    object_factories_[className] = [](RpcObjectId id) {
        // This factory creates a proxy object of type T
        return std::make_unique<T>(id);
    };
}

template<typename... Args>
RpcValue RpcSystem::Call(const std::string& funcName, Args... args) {
    // A standalone function call is treated as a method call on a "null" object (ID 0)
    return InternalCall(0, funcName, {args...});
}

template<typename... Args>
RpcValue RpcSystem::CallMethod(RpcObjectId objId, const std::string& methodName, Args... args) {
    return InternalCall(objId, methodName, {args...});
}

template<class F, class... Args>
void RpcSystem::EnqueueTask(F&& f, Args&&... args) {
    auto task = std::bind(std::forward<F>(f), std::forward<Args>(args)...);
    {
        std::unique_lock<std::mutex> lock(thread_pool_mutex_);
        tasks_.emplace(task);
    }
    thread_pool_cv_.notify_one();
}

inline RpcValue RpcSystem::InternalCall(RpcObjectId objId, const std::string& funcName, const std::vector<RpcValue>& args) {
    if (!IsConnected()) {
        throw std::runtime_error("RPC system is not connected.");
    }

    uint32_t callId;
    auto pendingCall = std::make_shared<PendingCall>();
    {
        std::lock_guard<std::mutex> lock(pending_calls_mutex_);
        callId = next_call_id_++;
        pending_calls_[callId] = pendingCall;
    }
    
    // {
    //     std::lock_guard<std::mutex> lock(cout_mutex_);
    //     std::cout << "RPC InternalCall to " << (objId == 0 ? "" : std::to_string(objId) + ".") << funcName << " with callId " << callId << std::endl;
    // }
    
    // Determine the full function name ("ObjectID.MethodName" or just "FunctionName")
    std::string remote_func_name = (objId == 0) ? funcName : std::to_string(objId) + "." + funcName;

    std::vector<char> buffer;
    char msg_type = 'C'; // 'C' for Call
    buffer.push_back(msg_type);
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&callId), reinterpret_cast<const char*>(&callId) + sizeof(callId));
    
    uint32_t name_len = remote_func_name.length();
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&name_len), reinterpret_cast<const char*>(&name_len) + sizeof(name_len));
    buffer.insert(buffer.end(), remote_func_name.begin(), remote_func_name.end());

    uint32_t arg_count = args.size();
    buffer.insert(buffer.end(), reinterpret_cast<const char*>(&arg_count), reinterpret_cast<const char*>(&arg_count) + sizeof(arg_count));
    for (const auto& arg : args) {
        Serializer::serialize(buffer, arg);
    }

    SendRPCMessage(buffer);
    // Wait for the return value
    std::unique_lock<std::mutex> lock(pendingCall->mtx);
    if (!pendingCall->cv.wait_for(lock, std::chrono::seconds(60), [&]{ return pendingCall->completed; })) {
        {
            std::lock_guard<std::mutex> pc_lock(pending_calls_mutex_);
            pending_calls_.erase(callId);
        }
        throw std::runtime_error("RPC call timed out for: " + remote_func_name);
    }
    
    return pendingCall->returnValue;
}


#endif // RPC_CORE_H
