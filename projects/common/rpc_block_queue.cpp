#include "rpc_interfaces.h"
#include "shared_memory_block.h"
#include <map>
#include <mutex>
#include <vector>
#include <memory>

struct ClientBlockQueue {
    vr::PropertyContainerHandle_t queueHandle;
    std::string path;
    uint32_t blockDataSize;
    uint32_t blockHeaderSize;
    uint32_t blockCount;
    uint32_t totalBlockSize;
    SharedMemoryBlock shm;
    
    struct BlockInfo {
        uint32_t blockIndex;
        bool isWrite;
    };
    std::map<vr::PropertyContainerHandle_t, BlockInfo> activeBlocks;
};

static std::mutex g_clientBlockQueuesMutex;
static std::map<vr::PropertyContainerHandle_t, std::shared_ptr<ClientBlockQueue>> g_clientBlockQueues;

static std::string SafeShmName(const std::string& path) {
    std::string res = path;
    for (char& c : res) {
        if (!isalnum((unsigned char)c)) {
            c = '_';
        }
    }
    return "ignition_bq_" + res;
}

struct DriverBlockQueue {
    vr::PropertyContainerHandle_t realQueueHandle;
    std::string path;
    uint32_t blockDataSize;
    uint32_t blockHeaderSize;
    uint32_t blockCount;
    uint32_t totalBlockSize;
    SharedMemoryBlock shm;
    std::map<vr::PropertyContainerHandle_t, uint32_t> activeReadBlocks;
    std::map<vr::PropertyContainerHandle_t, std::pair<void*, uint32_t>> activeWriteBlocks; // handle -> {realBuffer, index}
};

static std::mutex g_driverBlockQueuesMutex;
static std::map<vr::PropertyContainerHandle_t, std::shared_ptr<DriverBlockQueue>> g_driverBlockQueues;
static std::map<std::string, std::shared_ptr<DriverBlockQueue>> g_driverBlockQueuesByPath;

RpcBlockQueue::RpcBlockQueue(vr::IVRBlockQueue* real) : RpcObject(), real_block_queue_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_BlockQueue_Create, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 5) return RpcValue(std::vector<char>());
            std::string path = args[0].asString();
            uint32_t unBlockDataSize = (uint32_t)args[1].asInt();
            uint32_t unBlockHeaderSize = (uint32_t)args[2].asInt();
            uint32_t unBlockCount = (uint32_t)args[3].asInt();
            uint32_t unFlags = (uint32_t)args[4].asInt();

            vr::PropertyContainerHandle_t realQueueHandle = 0;
            int err = this->Create(&realQueueHandle, path.c_str(), unBlockDataSize, unBlockHeaderSize, unBlockCount, unFlags);
            if (err == 0 && realQueueHandle != 0) {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto q = std::make_shared<DriverBlockQueue>();
                q->realQueueHandle = realQueueHandle;
                q->path = path;
                q->blockDataSize = unBlockDataSize;
                q->blockHeaderSize = unBlockHeaderSize;
                q->blockCount = unBlockCount;
                q->totalBlockSize = unBlockDataSize + unBlockHeaderSize;
                
                size_t size = (size_t)q->totalBlockSize * unBlockCount;
                q->shm.Create(SafeShmName(path), size);

                g_driverBlockQueues[realQueueHandle] = q;
                g_driverBlockQueuesByPath[path] = q;
            }

            std::vector<char> res;
            res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
            uint64_t handle = (uint64_t)realQueueHandle;
            res.insert(res.end(), (char*)&handle, (char*)&handle + sizeof(uint64_t));
            return RpcValue(res);
        });

        this->RegisterFunction(RPCFunction_BlockQueue_Connect, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 1) return RpcValue(std::vector<char>());
            std::string path = args[0].asString();

            vr::PropertyContainerHandle_t realQueueHandle = 0;
            int err = this->Connect(&realQueueHandle, path.c_str());

            uint32_t dataSize = 0;
            uint32_t headerSize = 0;
            uint32_t count = 0;

            if (err == 0 && realQueueHandle != 0) {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = g_driverBlockQueuesByPath.find(path);
                if (it != g_driverBlockQueuesByPath.end()) {
                    auto q = it->second;
                    dataSize = q->blockDataSize;
                    headerSize = q->blockHeaderSize;
                    count = q->blockCount;
                    g_driverBlockQueues[realQueueHandle] = q; // Map handle as well
                }
            }

            std::vector<char> res;
            res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
            uint64_t handle = (uint64_t)realQueueHandle;
            res.insert(res.end(), (char*)&handle, (char*)&handle + sizeof(uint64_t));
            res.insert(res.end(), (char*)&dataSize, (char*)&dataSize + sizeof(uint32_t));
            res.insert(res.end(), (char*)&headerSize, (char*)&headerSize + sizeof(uint32_t));
            res.insert(res.end(), (char*)&count, (char*)&count + sizeof(uint32_t));
            return RpcValue(res);
        });

        this->RegisterFunction(RPCFunction_BlockQueue_Destroy, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 1) return RpcValue(false);
            uint64_t queueHandle = args[0].asUint64();

            std::shared_ptr<DriverBlockQueue> q;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = g_driverBlockQueues.find((vr::PropertyContainerHandle_t)queueHandle);
                if (it != g_driverBlockQueues.end()) {
                    q = it->second;
                    g_driverBlockQueues.erase(it);
                    g_driverBlockQueuesByPath.erase(q->path);
                }
            }

            if (q) {
                q->shm.Close();
                int err = this->Destroy(q->realQueueHandle);
                return RpcValue(err == 0);
            }
            return RpcValue(false);
        });

        this->RegisterFunction(RPCFunction_BlockQueue_AcquireWriteOnlyBlock, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 1) return RpcValue(std::vector<char>());
            uint64_t queueHandle = args[0].asUint64();

            std::shared_ptr<DriverBlockQueue> q;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = g_driverBlockQueues.find((vr::PropertyContainerHandle_t)queueHandle);
                if (it != g_driverBlockQueues.end()) {
                    q = it->second;
                }
            }

            if (!q || !q->shm.GetPointer()) {
                std::vector<char> res;
                int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
                res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
                return RpcValue(res);
            }

            uint32_t idx = 0;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                for (; idx < q->blockCount; ++idx) {
                    bool busy = false;
                    for (auto const& [hb, info] : q->activeWriteBlocks) {
                        if (info.second == idx) { busy = true; break; }
                    }
                    if (!busy) break;
                }
            }

            if (idx >= q->blockCount) {
                std::vector<char> res;
                int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
                res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
                return RpcValue(res);
            }

            vr::PropertyContainerHandle_t realBlockHandle = 0;
            void* realBuffer = nullptr;
            
            int err = this->AcquireWriteOnlyBlock(q->realQueueHandle, &realBlockHandle, &realBuffer);
            if (err == 0 && realBuffer) {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                q->activeWriteBlocks[realBlockHandle] = {realBuffer, idx};
            }

            std::vector<char> res;
            res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
            uint64_t block_handle_val = (uint64_t)realBlockHandle;
            res.insert(res.end(), (char*)&block_handle_val, (char*)&block_handle_val + sizeof(uint64_t));
            res.insert(res.end(), (char*)&idx, (char*)&idx + sizeof(uint32_t));
            return RpcValue(res);
        });

        this->RegisterFunction(RPCFunction_BlockQueue_ReleaseWriteOnlyBlock, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 3) return RpcValue(false);
            uint64_t queueHandle = args[0].asUint64();
            uint64_t blockHandle = args[1].asUint64();
            uint32_t blockIndex = (uint32_t)args[2].asInt();

            std::shared_ptr<DriverBlockQueue> q;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = g_driverBlockQueues.find((vr::PropertyContainerHandle_t)queueHandle);
                if (it != g_driverBlockQueues.end()) {
                    q = it->second;
                }
            }

            if (q && q->shm.GetPointer()) {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = q->activeWriteBlocks.find((vr::PropertyContainerHandle_t)blockHandle);
                if (it != q->activeWriteBlocks.end()) {
                    void* realBuffer = it->second.first;
                    q->activeWriteBlocks.erase(it);

                    char* shm_ptr = (char*)q->shm.GetPointer();
                    memcpy(realBuffer, shm_ptr + blockIndex * q->totalBlockSize, q->totalBlockSize);
                    this->ReleaseWriteOnlyBlock(q->realQueueHandle, (vr::PropertyContainerHandle_t)blockHandle);
                    return RpcValue(true);
                }
            }
            return RpcValue(false);
        });

        this->RegisterFunction(RPCFunction_BlockQueue_AcquireReadOnlyBlock, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 3) return RpcValue(std::vector<char>());
            uint64_t queueHandle = args[0].asUint64();
            int eReadType = args[1].asInt();
            uint32_t unTimeoutMs = (uint32_t)args[2].asInt();

            std::shared_ptr<DriverBlockQueue> q;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = g_driverBlockQueues.find((vr::PropertyContainerHandle_t)queueHandle);
                if (it != g_driverBlockQueues.end()) {
                    q = it->second;
                }
            }

            if (!q || !q->shm.GetPointer()) {
                std::vector<char> res;
                int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
                res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
                return RpcValue(res);
            }

            uint32_t idx = 0;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                for (; idx < q->blockCount; ++idx) {
                    bool busy = false;
                    for (auto const& [hb, i] : q->activeReadBlocks) {
                        if (i == idx) { busy = true; break; }
                    }
                    if (!busy) break;
                }
            }

            if (idx >= q->blockCount) {
                std::vector<char> res;
                int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
                res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
                return RpcValue(res);
            }

            vr::PropertyContainerHandle_t realBlockHandle = 0;
            void* realBuffer = nullptr;
            
            int err = this->WaitAndAcquireReadOnlyBlock(q->realQueueHandle, &realBlockHandle, &realBuffer, (vr::EBlockQueueReadType)eReadType, unTimeoutMs);
            if (err == 0 && realBuffer) {
                char* shm_ptr = (char*)q->shm.GetPointer();
                memcpy(shm_ptr + idx * q->totalBlockSize, realBuffer, q->totalBlockSize);
                
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                q->activeReadBlocks[realBlockHandle] = idx;
            }

            std::vector<char> res;
            res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
            uint64_t block_handle_val = (uint64_t)realBlockHandle;
            res.insert(res.end(), (char*)&block_handle_val, (char*)&block_handle_val + sizeof(uint64_t));
            res.insert(res.end(), (char*)&idx, (char*)&idx + sizeof(uint32_t));
            return RpcValue(res);
        });

        this->RegisterFunction(RPCFunction_BlockQueue_ReleaseReadOnlyBlock, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 2) return RpcValue(false);
            uint64_t queueHandle = args[0].asUint64();
            uint64_t blockHandle = args[1].asUint64();

            std::shared_ptr<DriverBlockQueue> q;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = g_driverBlockQueues.find((vr::PropertyContainerHandle_t)queueHandle);
                if (it != g_driverBlockQueues.end()) {
                    q = it->second;
                }
            }

            if (q) {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = q->activeReadBlocks.find((vr::PropertyContainerHandle_t)blockHandle);
                if (it != q->activeReadBlocks.end()) {
                    q->activeReadBlocks.erase(it);
                    int err = this->ReleaseReadOnlyBlock(q->realQueueHandle, (vr::PropertyContainerHandle_t)blockHandle);
                    return RpcValue(err == 0);
                }
            }
            return RpcValue(false);
        });

        this->RegisterFunction(RPCFunction_BlockQueue_QueueHasReader, [this](const std::vector<RpcValue>& args) {
            if (args.size() < 1) return RpcValue(std::vector<char>());
            uint64_t queueHandle = args[0].asUint64();

            std::shared_ptr<DriverBlockQueue> q;
            {
                std::lock_guard<std::mutex> lock(g_driverBlockQueuesMutex);
                auto it = g_driverBlockQueues.find((vr::PropertyContainerHandle_t)queueHandle);
                if (it != g_driverBlockQueues.end()) {
                    q = it->second;
                }
            }

            bool hasReaders = false;
            int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
            if (q) {
                err = this->QueueHasReader(q->realQueueHandle, &hasReaders);
            }

            std::vector<char> res;
            res.insert(res.end(), (char*)&err, (char*)&err + sizeof(int));
            uint8_t has_readers_byte = hasReaders ? 1 : 0;
            res.insert(res.end(), (char*)&has_readers_byte, (char*)&has_readers_byte + sizeof(uint8_t));
            return RpcValue(res);
        });
    }
}

RpcBlockQueue::RpcBlockQueue(RpcObjectId id) : RpcObject(id) {}

RpcBlockQueue::~RpcBlockQueue() {}

RpcClassEnum RpcBlockQueue::GetRpcClassId() const {
    return RPCClassBlockQueue;
}

vr::EBlockQueueError RpcBlockQueue::Create(vr::PropertyContainerHandle_t *pulQueueHandle, const char *pchPath, uint32_t unBlockDataSize, uint32_t unBlockHeaderSize, uint32_t unBlockCount, uint32_t unFlags) {
    if (IsProxy()) {
        if (!pulQueueHandle) return vr::EBlockQueueError_BlockQueueError_InternalError;
        
        int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
        uint64_t handle = 0;
        
        if (RpcSystem::IsConnected()) {
            try {
                RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_Create, 
                                                 RpcValue(pchPath ? pchPath : ""), 
                                                 RpcValue((int)unBlockDataSize), 
                                                 RpcValue((int)unBlockHeaderSize), 
                                                 RpcValue((int)unBlockCount), 
                                                 RpcValue((int)unFlags));
                if (result.isByteArray()) {
                    auto bytes = result.asByteArray();
                    if (bytes.size() >= sizeof(int) + sizeof(uint64_t)) {
                        memcpy(&err, bytes.data(), sizeof(int));
                        memcpy(&handle, bytes.data() + sizeof(int), sizeof(uint64_t));
                    }
                }
            } catch (...) {
                return vr::EBlockQueueError_BlockQueueError_InternalError;
            }
        }

        if (err == 0 && handle != 0) {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto q = std::make_shared<ClientBlockQueue>();
            q->queueHandle = (vr::PropertyContainerHandle_t)handle;
            q->path = pchPath ? pchPath : "";
            q->blockDataSize = unBlockDataSize;
            q->blockHeaderSize = unBlockHeaderSize;
            q->blockCount = unBlockCount;
            q->totalBlockSize = unBlockDataSize + unBlockHeaderSize;
            
            size_t size = (size_t)q->totalBlockSize * unBlockCount;
            q->shm.Open(SafeShmName(q->path), size);

            g_clientBlockQueues[q->queueHandle] = q;
            *pulQueueHandle = q->queueHandle;
        }
        return (vr::EBlockQueueError)err;
    } else {
        return real_block_queue_->Create(pulQueueHandle, pchPath, unBlockDataSize, unBlockHeaderSize, unBlockCount, unFlags);
    }
}

vr::EBlockQueueError RpcBlockQueue::Connect(vr::PropertyContainerHandle_t *pulQueueHandle, const char *pchPath) {
    if (IsProxy()) {
        if (!pulQueueHandle) return vr::EBlockQueueError_BlockQueueError_InternalError;
        
        int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
        uint64_t handle = 0;
        uint32_t dataSize = 0;
        uint32_t headerSize = 0;
        uint32_t count = 0;

        if (RpcSystem::IsConnected()) {
            try {
                RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_Connect, 
                                                 RpcValue(pchPath ? pchPath : ""));
                if (result.isByteArray()) {
                    auto bytes = result.asByteArray();
                    if (bytes.size() >= sizeof(int) + sizeof(uint64_t) + sizeof(uint32_t)*3) {
                        const char* ptr = bytes.data();
                        memcpy(&err, ptr, sizeof(int)); ptr += sizeof(int);
                        memcpy(&handle, ptr, sizeof(uint64_t)); ptr += sizeof(uint64_t);
                        memcpy(&dataSize, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
                        memcpy(&headerSize, ptr, sizeof(uint32_t)); ptr += sizeof(uint32_t);
                        memcpy(&count, ptr, sizeof(uint32_t));
                    }
                }
            } catch (...) {
                return vr::EBlockQueueError_BlockQueueError_InternalError;
            }
        }

        if (err == 0 && handle != 0) {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto q = std::make_shared<ClientBlockQueue>();
            q->queueHandle = (vr::PropertyContainerHandle_t)handle;
            q->path = pchPath ? pchPath : "";
            q->blockDataSize = dataSize;
            q->blockHeaderSize = headerSize;
            q->blockCount = count;
            q->totalBlockSize = dataSize + headerSize;

            size_t size = (size_t)q->totalBlockSize * count;
            q->shm.Open(SafeShmName(q->path), size);

            g_clientBlockQueues[q->queueHandle] = q;
            *pulQueueHandle = q->queueHandle;
        }
        return (vr::EBlockQueueError)err;
    } else {
        return real_block_queue_->Connect(pulQueueHandle, pchPath);
    }
}

vr::EBlockQueueError RpcBlockQueue::Destroy(vr::PropertyContainerHandle_t ulQueueHandle) {
    if (IsProxy()) {
        std::shared_ptr<ClientBlockQueue> q;
        {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto it = g_clientBlockQueues.find(ulQueueHandle);
            if (it != g_clientBlockQueues.end()) {
                q = it->second;
                g_clientBlockQueues.erase(it);
            }
        }

        if (q) {
            q->shm.Close();
            if (RpcSystem::IsConnected()) {
                try {
                    RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_Destroy, RpcValue((uint64_t)ulQueueHandle));
                } catch (...) {}
            }
        }
        return vr::EBlockQueueError_BlockQueueError_None;
    } else {
        return real_block_queue_->Destroy(ulQueueHandle);
    }
}

vr::EBlockQueueError RpcBlockQueue::AcquireWriteOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t *pulBlockHandle, void **ppvBuffer) {
    if (IsProxy()) {
        if (!pulBlockHandle || !ppvBuffer) return vr::EBlockQueueError_BlockQueueError_InternalError;

        std::shared_ptr<ClientBlockQueue> q;
        {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto it = g_clientBlockQueues.find(ulQueueHandle);
            if (it != g_clientBlockQueues.end()) {
                q = it->second;
            }
        }

        if (!q || !q->shm.GetPointer()) {
            return vr::EBlockQueueError_BlockQueueError_InternalError;
        }

        int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
        uint64_t realBlockHandle = 0;
        uint32_t idx = 0;

        if (RpcSystem::IsConnected()) {
            try {
                RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_AcquireWriteOnlyBlock, 
                                                 RpcValue((uint64_t)ulQueueHandle));
                if (result.isByteArray()) {
                    auto bytes = result.asByteArray();
                    if (bytes.size() >= sizeof(int)) {
                        memcpy(&err, bytes.data(), sizeof(int));
                        if (err == 0 && bytes.size() >= sizeof(int) + sizeof(uint64_t) + sizeof(uint32_t)) {
                            memcpy(&realBlockHandle, bytes.data() + sizeof(int), sizeof(uint64_t));
                            memcpy(&idx, bytes.data() + sizeof(int) + sizeof(uint64_t), sizeof(uint32_t));
                        }
                    }
                }
            } catch (...) {
                return vr::EBlockQueueError_BlockQueueError_InternalError;
            }
        }

        if (err == 0 && realBlockHandle != 0) {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            ClientBlockQueue::BlockInfo info = { idx, true };
            q->activeBlocks[(vr::PropertyContainerHandle_t)realBlockHandle] = info;

            *pulBlockHandle = (vr::PropertyContainerHandle_t)realBlockHandle;
            char* shm_ptr = (char*)q->shm.GetPointer();
            *ppvBuffer = shm_ptr + idx * q->totalBlockSize;
        }

        return (vr::EBlockQueueError)err;
    } else {
        return real_block_queue_->AcquireWriteOnlyBlock(ulQueueHandle, pulBlockHandle, ppvBuffer);
    }
}

vr::EBlockQueueError RpcBlockQueue::ReleaseWriteOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t ulBlockHandle) {
    if (IsProxy()) {
        std::shared_ptr<ClientBlockQueue> q;
        {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto it = g_clientBlockQueues.find(ulQueueHandle);
            if (it != g_clientBlockQueues.end()) {
                q = it->second;
            }
        }

        if (!q) return vr::EBlockQueueError_BlockQueueError_InternalError;

        uint32_t idx = 0;
        bool found = false;
        {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto it = q->activeBlocks.find(ulBlockHandle);
            if (it != q->activeBlocks.end()) {
                idx = it->second.blockIndex;
                q->activeBlocks.erase(it);
                found = true;
            }
        }

        if (!found) return vr::EBlockQueueError_BlockQueueError_InternalError;

        // The driver will now copy from shared memory
        if (RpcSystem::IsConnected()) {
            try {
                RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_ReleaseWriteOnlyBlock, RpcValue((uint64_t)ulQueueHandle), RpcValue((uint64_t)ulBlockHandle), RpcValue((int)idx));
            } catch (...) {}
        }
        return vr::EBlockQueueError_BlockQueueError_None;
    } else {
        return real_block_queue_->ReleaseWriteOnlyBlock(ulQueueHandle, ulBlockHandle);
    }
}

vr::EBlockQueueError RpcBlockQueue::WaitAndAcquireReadOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t *pulBlockHandle, void **ppvBuffer, vr::EBlockQueueReadType eReadType, uint32_t unTimeoutMs) {
    if (IsProxy()) {
        if (!pulBlockHandle || !ppvBuffer) return vr::EBlockQueueError_BlockQueueError_InternalError;

        std::shared_ptr<ClientBlockQueue> q;
        {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto it = g_clientBlockQueues.find(ulQueueHandle);
            if (it != g_clientBlockQueues.end()) {
                q = it->second;
            }
        }

        if (!q || !q->shm.GetPointer()) {
            return vr::EBlockQueueError_BlockQueueError_InternalError;
        }

        int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
        uint64_t realBlockHandle = 0;
        uint32_t idx = 0;

        if (RpcSystem::IsConnected()) {
            try {
                RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_AcquireReadOnlyBlock, 
                                                 RpcValue((uint64_t)ulQueueHandle), 
                                                 RpcValue((int)eReadType), 
                                                 RpcValue((int)unTimeoutMs));
                if (result.isByteArray()) {
                    auto bytes = result.asByteArray();
                    if (bytes.size() >= sizeof(int)) {
                        memcpy(&err, bytes.data(), sizeof(int));
                        if (err == 0 && bytes.size() >= sizeof(int) + sizeof(uint64_t) + sizeof(uint32_t)) {
                            memcpy(&realBlockHandle, bytes.data() + sizeof(int), sizeof(uint64_t));
                            memcpy(&idx, bytes.data() + sizeof(int) + sizeof(uint64_t), sizeof(uint32_t));
                        }
                    }
                }
            } catch (...) {
                return vr::EBlockQueueError_BlockQueueError_InternalError;
            }
        }

        if (err == 0 && realBlockHandle != 0) {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            ClientBlockQueue::BlockInfo info = { idx, false };
            q->activeBlocks[(vr::PropertyContainerHandle_t)realBlockHandle] = info;

            *pulBlockHandle = (vr::PropertyContainerHandle_t)realBlockHandle;
            char* shm_ptr = (char*)q->shm.GetPointer();
            *ppvBuffer = shm_ptr + idx * q->totalBlockSize;
        }

        return (vr::EBlockQueueError)err;
    } else {
        return real_block_queue_->WaitAndAcquireReadOnlyBlock(ulQueueHandle, pulBlockHandle, ppvBuffer, eReadType, unTimeoutMs);
    }
}

vr::EBlockQueueError RpcBlockQueue::AcquireReadOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t *pulBlockHandle, void **ppvBuffer, vr::EBlockQueueReadType eReadType) {
    return WaitAndAcquireReadOnlyBlock(ulQueueHandle, pulBlockHandle, ppvBuffer, eReadType, 0);
}

vr::EBlockQueueError RpcBlockQueue::ReleaseReadOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t ulBlockHandle) {
    if (IsProxy()) {
        std::shared_ptr<ClientBlockQueue> q;
        {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto it = g_clientBlockQueues.find(ulQueueHandle);
            if (it != g_clientBlockQueues.end()) {
                q = it->second;
            }
        }

        if (!q) return vr::EBlockQueueError_BlockQueueError_InternalError;

        bool found = false;
        {
            std::lock_guard<std::mutex> lock(g_clientBlockQueuesMutex);
            auto it = q->activeBlocks.find(ulBlockHandle);
            if (it != q->activeBlocks.end()) {
                q->activeBlocks.erase(it);
                found = true;
            }
        }

        if (!found) return vr::EBlockQueueError_BlockQueueError_InternalError;

        if (RpcSystem::IsConnected()) {
            try {
                RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_ReleaseReadOnlyBlock, RpcValue((uint64_t)ulQueueHandle), RpcValue((uint64_t)ulBlockHandle));
            } catch (...) {}
        }
        return vr::EBlockQueueError_BlockQueueError_None;
    } else {
        return real_block_queue_->ReleaseReadOnlyBlock(ulQueueHandle, ulBlockHandle);
    }
}

vr::EBlockQueueError RpcBlockQueue::QueueHasReader(vr::PropertyContainerHandle_t ulQueueHandle, bool *pbHasReaders) {
    if (IsProxy()) {
        if (!pbHasReaders) return vr::EBlockQueueError_BlockQueueError_InternalError;
        
        int err = (int)vr::EBlockQueueError_BlockQueueError_InternalError;
        uint8_t hasReader = 0;

        if (RpcSystem::IsConnected()) {
            try {
                RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_BlockQueue_QueueHasReader, RpcValue((uint64_t)ulQueueHandle));
                if (result.isByteArray()) {
                    auto bytes = result.asByteArray();
                    if (bytes.size() >= sizeof(int) + sizeof(uint8_t)) {
                        memcpy(&err, bytes.data(), sizeof(int));
                        memcpy(&hasReader, bytes.data() + sizeof(int), sizeof(uint8_t));
                    }
                }
            } catch (...) {}
        }

        *pbHasReaders = (hasReader != 0);
        return (vr::EBlockQueueError)err;
    } else {
        return real_block_queue_->QueueHasReader(ulQueueHandle, pbHasReaders);
    }
}
