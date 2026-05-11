#include "rpc_core.h"
#include "rpc_interfaces.h"

#ifdef _WIN32
#include "wine_utils.h"
#endif

#include <cstdint>
#include <vector>

// --- RpcResources ---

RpcResources::RpcResources(vr::IVRResources* real) : RpcObject(), real_resources_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_Resources_LoadSharedResource, [this](const auto& args) {
            std::vector<char> buffer;

            const std::string resource_name = args[0].asString();
            buffer.resize(args[1].asUint64());

            uint32_t size = this->LoadSharedResource(resource_name.c_str(), buffer.data(), buffer.size());
            if (size > 0 && size <= buffer.size()) {
                return RpcValue(buffer);
            }
            return RpcValue(static_cast<uint64_t>(size));
        });
        
        this->RegisterFunction(RPCFunction_Resources_GetResourceFullPath, [this](const auto& args) {
            std::vector<char> buffer;
            const std::string resource_name = args[0].asString();
            const std::string resource_type = args[1].asString();
            buffer.resize(args[2].asUint64());

            uint32_t size = this->GetResourceFullPath(resource_name.c_str(), resource_type.c_str(), buffer.data(), buffer.size());
            if (size > 0 && size <= buffer.size()) {
                return RpcValue(buffer);
            }
            return RpcValue(static_cast<uint64_t>(size));
        });
    }
}
RpcResources::RpcResources(RpcObjectId id) : RpcObject(id) {}
RpcResources::~RpcResources() {}

RpcClassEnum RpcResources::GetRpcClassId() const {
    return RPCClassResources;
}

uint32_t RpcResources::LoadSharedResource(const char *pchResourceName, char *pchBuffer, uint32_t unBufferLen) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Resources_LoadSharedResource, RpcValue(std::string(pchResourceName)), RpcValue(static_cast<uint64_t>(unBufferLen)));
        if (result.isByteArray()) {
            std::vector<char> res = result.asByteArray();
            if (pchBuffer) {
                memcpy(pchBuffer, res.data(), res.size());
            }
            return res.size();
        }
        else {
            return static_cast<uint32_t>(result.asUint64());      
        }
    }
    else {
        return real_resources_->LoadSharedResource(pchResourceName, pchBuffer, unBufferLen);
    }
}

uint32_t RpcResources::GetResourceFullPath(const char *pchResourceName, const char *pchResourceTypeDirectory, char *pchPathBuffer, uint32_t unBufferLen) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Resources_GetResourceFullPath, RpcValue(std::string(pchResourceName)), RpcValue(std::string(pchResourceTypeDirectory)), RpcValue(static_cast<uint64_t>(unBufferLen)));
        if (result.isByteArray()) {
            std::vector<char> path = result.asByteArray();
            if (pchPathBuffer) {
                memcpy(pchPathBuffer, path.data(), path.size());
            }

#ifdef _WIN32
            if (IsRunningInWine()) {
                std::string unix_path(path.data(), path.size());
                std::string windows_path = WineGetDosFileName(unix_path);

                if (pchPathBuffer && unBufferLen > 0) {
                    memcpy(pchPathBuffer, windows_path.c_str(), std::min((size_t)unBufferLen, windows_path.size() + 1));
                }
                return windows_path.size() + 1;
            }
#endif

            return path.size();
        }
        else {
            return static_cast<uint32_t>(result.asUint64());
        }
    }
    else {
        return real_resources_->GetResourceFullPath(pchResourceName, pchResourceTypeDirectory, pchPathBuffer, unBufferLen);
    }
}
