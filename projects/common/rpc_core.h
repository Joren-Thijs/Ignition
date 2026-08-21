#pragma once

#include <string>
#include <vector>
#include <span>
#include <map>
#include <functional>
#include <memory>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <queue>
#include <utility>
#include <cstring>

#include "rpc_enums.h"

// Forward declarations
struct CircularBuffer;
class RpcSystem;
class RpcObject;
class RpcValue;

using RpcObjectId = uint64_t;
using RpcFunction = std::function<RpcValue(const std::vector<RpcValue>&)>;

constexpr size_t SHM_BUFFER_SIZE = 1024 * 1024; // 1MB

// --- Argument Serialization ---
// A type-erased container for arguments and return values.
class RpcValue {
public:
    RpcValue() : type_(T_NULL) {}
    explicit RpcValue(int v) : type_(T_INT), int_val_(v) {}
    explicit RpcValue(float v) : type_(T_FLOAT), float_val_(v) {}
    explicit RpcValue(double v) : type_(T_DOUBLE), double_val_(v) {}
    explicit RpcValue(uint64_t v) : type_(T_UINT64), uint64_val_(v) {}
    explicit RpcValue(std::vector<char> v) : type_(T_BYTE_ARRAY), byte_array_val_(std::move(v)) {}
    RpcValue(const char* data, size_t len) : type_(T_BYTE_ARRAY), byte_array_val_(data, data + len) {}
    RpcValue(RpcObject* v);

    explicit RpcValue(const std::string& v) : type_(T_BYTE_ARRAY) {
        byte_array_val_.assign(v.begin(), v.end());
    }

    // Copy constructor and assignment
    RpcValue(const RpcValue& other);
    RpcValue& operator=(const RpcValue& other);
    void swap(RpcValue& other) noexcept;
    ~RpcValue();

    // Type checking
    bool isInt() const { return type_ == T_INT; }
    bool isFloat() const { return type_ == T_FLOAT; }
    bool isDouble() const { return type_ == T_DOUBLE; }    
    bool isUint64() const { return type_ == T_UINT64; }
    bool isByteArray() const { return type_ == T_BYTE_ARRAY; }
    bool isObject() const { return type_ == T_OBJECT_REF; }

    // Value accessors
    int asInt() const;
    float asFloat() const;
    double asDouble() const;
    uint64_t asUint64() const;
    const std::vector<char>& asByteArray() const;
    RpcObject* asObject() const;

    // Helper to access byte array as std::string
    std::string asString() const;

private:
    friend class RpcSerializer;
    enum Type { T_NULL, T_INT, T_FLOAT, T_DOUBLE, T_BYTE_ARRAY, T_OBJECT_REF, T_UINT64 };
    Type type_;

    // Holds info to look up a remote object
    struct ObjectRef {
        RpcObjectId id;
        RpcClassEnum class_id;
    };

    union {
        int int_val_ = 0;
        float float_val_;
        double double_val_;
        uint64_t uint64_val_;
        ObjectRef obj_ref_;
    };

    // Value-based storage for variable-length data
    std::vector<char> byte_array_val_{};
};

// --- RPC System (Static Class) ---
class RpcSystem {
public:
    // Get the singleton instance
    static RpcSystem& GetInstance();

    // Must be called once at the start of the application
    static void Initialize(const std::string& ipcName) { GetInstance()._Initialize(ipcName); }
    static void InitializeThreadPool(size_t num_threads) { GetInstance()._InitializeThreadPool(num_threads); }

    // Register a class type and its proxy factory.
    template<typename T>
    static void RegisterRPCClass() { GetInstance()._RegisterRPCClass<T>(); }

    // Register a standalone function (not tied to an object)
    static void RegisterFunction(RpcFunctionEnum funcId, RpcFunction func) { GetInstance()._RegisterFunction(funcId, func); }

    // Unregister a standalone function
    static void UnregisterFunction(RpcFunctionEnum funcId) { GetInstance()._UnregisterFunction(funcId); }

    // Call a remote standalone function.
    template<typename... Args>
    static RpcValue Call(RpcFunctionEnum funcId, Args... args) { return GetInstance()._Call(funcId, args...); }

    // Call a remote method on an object.
    template<typename... Args>
    static RpcValue CallMethod(RpcObjectId objId, RpcFunctionEnum funcId, Args... args) { return GetInstance()._CallMethod(objId, funcId, args...); }

    static bool IsAlive() { return GetInstance()._IsAlive(); }

    static void CreateIPC() { GetInstance()._CreateIPC(); }
    static bool ConnectToExistingIPC() { return GetInstance()._ConnectToExistingIPC(); }

    static void Shutdown() { GetInstance()._Shutdown(); }
    static void ShutdownThreadPool() { GetInstance()._ShutdownThreadPool(); }
    static bool IsConnected() { return GetInstance()._IsConnected(); }
    
private:
    friend class RpcObject;
    friend class RpcValue;
    friend class RpcSerializer;
    // Singleton: private constructor, destructor, no copy/move
    RpcSystem();
    ~RpcSystem();
    RpcSystem(const RpcSystem&) = delete;
    RpcSystem& operator=(const RpcSystem&) = delete;

    struct PendingCall {
        std::condition_variable cv;
        std::mutex mtx;
        RpcValue returnValue;
        bool completed = false;
    };

    using ObjectFactory = std::function<std::unique_ptr<RpcObject>(RpcObjectId)>;

    void _Initialize(const std::string& ipcName);
    void _InitializeThreadPool(size_t num_threads);
    template<typename T> void _RegisterRPCClass();
    template<typename... Args> RpcValue _Call(const std::string& funcName, Args... args);
    template<typename... Args> RpcValue _Call(RpcFunctionEnum funcId, Args... args);
    template<typename... Args> RpcValue _CallMethod(RpcObjectId objId, RpcFunctionEnum funcId, Args... args);
    void _CreateIPC();
    bool _ConnectToExistingIPC();
    void _Shutdown();
    void _ShutdownThreadPool();
    bool _IsAlive();
    bool _IsConnected();

    RpcObjectId _GenerateObjectId();
    void _RegisterLocalObject(RpcObject* obj);
    void _UnregisterLocalObject(RpcObjectId id);
    void _RegisterFunction(RpcFunctionEnum funcId, RpcFunction func);
    void _UnregisterFunction(RpcFunctionEnum funcId);
    RpcFunction _FindFunction(RpcFunctionEnum funcId);
    RpcObject* _GetLocalObject(RpcObjectId id);
    RpcObject* _FindOrCreateProxy(RpcObjectId id, RpcClassEnum classId);

    void ListenLoop();
    void ProcessMessage(std::span<const char> buffer);
    void SendRPCMessage(const std::vector<char>& buffer);
    template<class F, class... Args>
    void EnqueueTask(F&& f, Args&&... args);

    RpcValue InternalCall(RpcObjectId objId, RpcFunctionEnum funcId, const std::vector<RpcValue>& args);

    std::string pipe_name_;
    bool is_server_ = false;
    std::unique_ptr<std::thread> listen_thread_;
    std::atomic<bool> running_ = false;

    CircularBuffer* pC2S_Buffer_ = nullptr;
    CircularBuffer* pS2C_Buffer_ = nullptr;

    // --- Function and Object Registries ---
    std::atomic<RpcObjectId> next_object_id_{1};
    std::map<RpcObjectId, RpcObject*> local_objects_; // Real objects this process owns
    std::map<RpcObjectId, std::unique_ptr<RpcObject>> remote_proxies_; // Proxies to remote objects    
    std::map<RpcFunctionEnum, RpcFunction> static_function_registry_;
    std::map<RpcClassEnum, ObjectFactory> object_factories_;
    std::mutex object_mutex_;

    // --- Synchronization for Calls ---
    std::map<uint32_t, std::shared_ptr<PendingCall>> pending_calls_;
    std::mutex pending_calls_mutex_;
    std::atomic<uint32_t> next_call_id_{1};

    // --- Thread Pool for handling calls ---
    std::vector<std::thread> worker_threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex thread_pool_mutex_;
    std::condition_variable thread_pool_cv_;
    bool stop_thread_pool_ = false;
};

// --- RPC Object Base Class ---
// Inherit from this class to make it usable over RPC.
class RpcObject {
public:
    // Constructor for a new local object. It will be registered with the system. It's inline because it's in a header.
    inline RpcObject() : is_proxy_(false) {
        object_id_ = RpcSystem::GetInstance()._GenerateObjectId();
        RpcSystem::GetInstance()._RegisterLocalObject(this);
    }

    // Constructor for a proxy to a remote object. Use this in derived classes.
    inline RpcObject(RpcObjectId id) : object_id_(id), is_proxy_(true) {}

    inline virtual ~RpcObject() {
        if (!is_proxy_) {
            // If this is a real object, unregister it from the system
            // upon destruction.
            RpcSystem::GetInstance()._UnregisterLocalObject(object_id_);
        }
    }

    // Register function
    void RegisterFunction(RpcFunctionEnum funcId, RpcFunction func);
    void UnregisterFunction(RpcFunctionEnum funcId);
    RpcFunction FindFunction(RpcFunctionEnum funcId);

    RpcObjectId GetId() const { return object_id_; }
    bool IsProxy() const { return is_proxy_; }

    // Must be implemented by derived classes to identify the type for proxy creation.
    virtual RpcClassEnum GetRpcClassId() const = 0;

protected:
    std::map<RpcFunctionEnum, RpcFunction> function_registry_;
    std::mutex registry_mutex_;

    RpcObjectId object_id_;
    const bool is_proxy_;
};

// Simple serializer
class RpcSerializer {
public:
    static void Serialize(std::vector<char>& buffer, const RpcValue& val);
    static RpcValue Deserialize(const char*& bufferPtr, const char* bufferEnd);
};

// --- Template & Inline Implementations ---

template<typename T>
void RpcSystem::_RegisterRPCClass() {
    T dummy_for_id(1); // Create a dummy proxy to get the class ID
    RpcClassEnum classId = dummy_for_id.GetRpcClassId();

    std::lock_guard<std::mutex> lock(object_mutex_);
    object_factories_[classId] = [](RpcObjectId id) {
        // This factory creates a proxy object of type T
        return std::make_unique<T>(id);
    };
}

template<typename... Args>
RpcValue RpcSystem::_Call(RpcFunctionEnum funcId, Args... args) {
    return this->_CallMethod(0, funcId, args...);
}

template<typename... Args>
RpcValue RpcSystem::_CallMethod(RpcObjectId objId, RpcFunctionEnum funcId, Args... args) {
    return InternalCall(objId, funcId, {RpcValue(args)...});
}

template<class F, class... Args>
void RpcSystem::EnqueueTask(F&& f, Args&&... args) {
    {
        std::unique_lock<std::mutex> lock(thread_pool_mutex_);
        tasks_.emplace(std::forward<F>(f));
    }
    thread_pool_cv_.notify_one();
}