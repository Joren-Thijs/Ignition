#include "rpc_interfaces.h"

// --- RpcResources ---

RpcResources::RpcResources(vr::IVRResources* real) : RpcObject(), real_resources_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_Resources_LoadSharedResource, [this](const auto& args) {
            char buffer[4096]; // A reasonable max size for a resource
            const std::string resource_name = args[0].asString();

            uint32_t result = this->LoadSharedResource(resource_name.c_str(), buffer, sizeof(buffer));
            if (result > 0 && result <= sizeof(buffer)) {
                return RpcValue(std::string(buffer));
            }
            return RpcValue();
        });
        
        this->RegisterFunction(RPCFunction_Resources_GetResourceFullPath, [this](const auto& args) {
            char buffer[4096];
            const std::string resource_name = args[0].asString();
            const std::string resource_type = args[1].asString();
            uint32_t result = this->GetResourceFullPath(resource_name.c_str(), resource_type.c_str(), buffer, sizeof(buffer));
            if (result > 0 && result <= sizeof(buffer)) {
                return RpcValue(std::string(buffer));
            }
            return RpcValue();
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
        if (unBufferLen > 4096) {
            std::cout << "Warning: LoadSharedResource called with unBufferLen > 4096. You probably will have some issues..." << std::endl;
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Resources_LoadSharedResource, RpcValue(std::string(pchResourceName)));
        if (result.isString()) {
            std::string res = result.asString();
            if (pchBuffer && unBufferLen > 0) {
                memcpy(pchBuffer, res.c_str(), std::min((size_t)unBufferLen - 1, res.size()));
                pchBuffer[std::min((size_t)unBufferLen - 1, res.size())] = '\0';
            }
            return res.length() + 1;
        }
        return 0;
    }
    else {
        return real_resources_->LoadSharedResource(pchResourceName, pchBuffer, unBufferLen);
    }
}

uint32_t RpcResources::GetResourceFullPath(const char *pchResourceName, const char *pchResourceTypeDirectory, char *pchPathBuffer, uint32_t unBufferLen) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Resources_GetResourceFullPath, RpcValue(std::string(pchResourceName)), RpcValue(std::string(pchResourceTypeDirectory)));
        if (result.isString()) {
            std::string path = result.asString();
            if (pchPathBuffer && unBufferLen > 0) {
                memcpy(pchPathBuffer, path.c_str(), std::min((size_t)unBufferLen - 1, path.size()));
                pchPathBuffer[std::min((size_t)unBufferLen - 1, path.size())] = '\0';
            }
            return path.length() + 1;
        }
        return 0;
    }
    else {
        return real_resources_->GetResourceFullPath(pchResourceName, pchResourceTypeDirectory, pchPathBuffer, unBufferLen);
    }
}
