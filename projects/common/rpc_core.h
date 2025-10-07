#pragma once

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
#include <cstring>

#include "rpc_enums.h"

// Forward declarations
struct CircularBuffer;
class RpcSystem; // Now a static class
class RpcObject;

// Unique identifier for an RPC-enabled object instance
using RpcObjectId = uint64_t;

constexpr size_t SHM_BUFFER_SIZE = 1024 * 1024; // 1MB

// --- Argument Serialization ---
// A type-erased container for arguments and return values.
class RpcValue {
public:
    RpcValue() : type_(T_NULL) {}
    explicit RpcValue(int v) : type_(T_INT), int_val_(v) {}
    explicit RpcValue(float v) : type_(T_FLOAT), float_val_(v) {}
    explicit RpcValue(double v) : type_(T_DOUBLE), double_val_(v) {}
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
    bool isDouble() const { return type_ == T_DOUBLE; }
    bool isString() const { return type_ == T_STRING; }
    bool isPointer() const { return type_ == T_POINTER; }
    bool isUint64() const { return type_ == T_UINT64; }
    bool isObject() const { return type_ == T_OBJECT_REF; }

    // Value accessors
    int asInt() const;
    float asFloat() const;
    double asDouble() const;
    std::string asString() const;
    uint64_t asUint64() const;
    std::pair<const char*, size_t> asPointer() const;
    RpcObject* asObject() const; // RpcSystem is now static

private:
    friend class Serializer;
    enum Type { T_NULL, T_INT, T_FLOAT, T_DOUBLE, T_STRING, T_POINTER, T_OBJECT_REF, T_UINT64 };
    Type type_;

    // Holds info to look up a remote object
    struct ObjectRef {
        RpcObjectId id;
        RpcClassEnum class_id; // Used to create proxies
    };

    union {
        int int_val_ = 0;
        float float_val_;
        double double_val_;
        uint64_t uint64_val_;
        ObjectRef obj_ref_;
    };
    // Value-based storage for variable-length data
    std::string str_val_{};
    std::vector<char> ptr_val_{};
};

// --- RPC System (Static Class) ---
class RpcSystem {
public:
    using RpcFunction = std::function<RpcValue(const std::vector<RpcValue>&)>;

    // Get the singleton instance
    static RpcSystem& GetInstance();

    // Must be called once at the start of the application
    static void Initialize(const std::string& pipeName) { GetInstance()._Initialize(pipeName); }
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
    template<typename... Args> static RpcValue CallMethod(RpcObjectId objId, RpcFunctionEnum funcId, Args... args) { return GetInstance()._CallMethod(objId, funcId, args...); }

    static void StartServer() { GetInstance()._StartServer(); }
    static bool ConnectToServer() { return GetInstance()._ConnectToServer(); }

    static void Shutdown() { GetInstance()._Shutdown(); }
    static void ShutdownThreadPool() { GetInstance()._ShutdownThreadPool(); }
    static bool IsConnected() { return GetInstance()._IsConnected(); }

    // --- Object Management (public for RpcObject and RpcValue) ---
    RpcObjectId _GenerateObjectId();
    void _RegisterLocalObject(RpcObject* obj);
    void _UnregisterLocalObject(RpcObjectId id);
    void _RegisterFunction(RpcFunctionEnum funcId, RpcFunction func);
    void _UnregisterFunction(RpcFunctionEnum funcId);
    RpcFunction _FindFunction(RpcFunctionEnum funcId);
    RpcObject* _GetLocalObject(RpcObjectId id);
    RpcObject* _FindOrCreateProxy(RpcObjectId id, RpcClassEnum classId);

private:
    friend class RpcObject;
    friend class Serializer;
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

    void _Initialize(const std::string& pipeName);
    void _InitializeThreadPool(size_t num_threads);
    template<typename T> void _RegisterRPCClass();
    template<typename... Args> RpcValue _Call(const std::string& funcName, Args... args);
    template<typename... Args> RpcValue _Call(RpcFunctionEnum funcId, Args... args);
    template<typename... Args> RpcValue _CallMethod(RpcObjectId objId, RpcFunctionEnum funcId, Args... args);
    void _StartServer();
    bool _ConnectToServer();
    void _Shutdown();
    void _ShutdownThreadPool();
    bool _IsConnected();

    // RPCObject helpers
    static RpcObjectId GenerateObjectId();
    static void RegisterLocalObject(RpcObject* obj);
    static void UnregisterLocalObject(RpcObjectId id);
    static RpcObject* GetLocalObject(RpcObjectId id);
    static RpcObject* FindOrCreateProxy(RpcObjectId id, RpcClassEnum classId);

    void ListenLoop();
    void ProcessMessage(const std::vector<char>& buffer);
    void SendRPCMessage(const std::vector<char>& buffer);
    template<class F, class... Args>
    void EnqueueTask(F&& f, Args&&... args);

    RpcValue InternalCall(RpcObjectId objId, RpcFunctionEnum funcId, const std::vector<RpcValue>& args);

    std::string pipe_name_;
    bool is_server_ = false;
    std::unique_ptr<std::thread> listen_thread_;
    bool running_ = false;

    CircularBuffer* pC2S_Buffer_ = nullptr;
    CircularBuffer* pS2C_Buffer_ = nullptr;

    // --- Function and Object Registries ---
    std::atomic<RpcObjectId> next_object_id_{1};
    std::map<RpcObjectId, RpcObject*> local_objects_; // Real objects this process owns
    std::map<RpcObjectId, std::unique_ptr<RpcObject>> remote_proxies_; // Proxies to remote objects    
    std::map<RpcFunctionEnum, std::function<RpcValue(const std::vector<RpcValue>&)>> static_function_registry_;
    std::map<RpcClassEnum, ObjectFactory> object_factories_;
    std::mutex object_mutex_;

    // --- Synchronization for Calls ---
    std::map<uint32_t, std::shared_ptr<PendingCall>> pending_calls_;
    std::mutex pending_calls_mutex_;
    std::atomic<uint32_t> next_call_id_{1};

    // --- Synchronous Sender ---
    std::mutex send_mutex_;
    // --- Thread Pool for handling calls ---
    std::vector<std::thread> worker_threads_;
    std::queue<std::function<void()>> tasks_;
    std::mutex thread_pool_mutex_;
    std::condition_variable thread_pool_cv_;
    bool stop_thread_pool_ = false;
    std::mutex cout_mutex_;
};

// --- RPC Object Base Class ---
// Inherit from this class to make it usable over RPC.
class RpcObject {
public:
    using RpcFunction = RpcSystem::RpcFunction;

    // Constructor for a new local object. It will be registered with the system. It's inline because it's in a header.
    inline RpcObject() : is_proxy_(false) {
        object_id_ = RpcSystem::GenerateObjectId();
        RpcSystem::RegisterLocalObject(this);
    }

    // Constructor for a proxy to a remote object. Use this in derived classes.
    inline RpcObject(RpcObjectId id) : object_id_(id), is_proxy_(true) {}

    inline virtual ~RpcObject() {
        if (!is_proxy_) {
            // If this is a real object, unregister it from the system
            // upon destruction.
            RpcSystem::UnregisterLocalObject(object_id_);
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
class Serializer {
public:
    static void serialize(std::vector<char>& buffer, const RpcValue& val);
    static RpcValue deserialize(const char*& buffer_ptr, const char* buffer_end);
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

// --- Global Object Management helpers ---
inline RpcObjectId RpcSystem::GenerateObjectId() { return RpcSystem::GetInstance()._GenerateObjectId(); }
inline void RpcSystem::RegisterLocalObject(RpcObject* obj) { RpcSystem::GetInstance()._RegisterLocalObject(obj); }
inline void RpcSystem::UnregisterLocalObject(RpcObjectId id) {RpcSystem:: GetInstance()._UnregisterLocalObject(id); }
inline RpcObject* RpcSystem::GetLocalObject(RpcObjectId id) { return RpcSystem::GetInstance()._GetLocalObject(id); }
inline RpcObject* RpcSystem::FindOrCreateProxy(RpcObjectId id, RpcClassEnum classId) { return RpcSystem::GetInstance()._FindOrCreateProxy(id, classId); }
