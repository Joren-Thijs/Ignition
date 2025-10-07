#include "rpc_interfaces.h"

// --- RpcDriverManager ---

RpcDriverManager::RpcDriverManager(vr::IVRDriverManager* real) : RpcObject(), real_manager_(real) {
    if (!IsProxy()) { // Real object on client
        this->RegisterFunction(RPCFunction_DriverManager_GetDriverCount, [this](const auto& args) {
            return RpcValue((int)this->GetDriverCount());
        });

        this->RegisterFunction(RPCFunction_DriverManager_GetDriverName, [this](const auto& args) {
            char buffer[1024];
            uint32_t result = this->GetDriverName((vr::DriverId_t)args[0].asInt(), buffer, sizeof(buffer));
            return RpcValue(std::string(buffer, result));
        });

        this->RegisterFunction(RPCFunction_DriverManager_GetDriverHandle, [this](const auto& args) {
            return RpcValue((int)this->GetDriverHandle(args[0].asString().c_str()));
        });
        
        this->RegisterFunction(RPCFunction_DriverManager_IsEnabled, [this](const auto& args) {
            return RpcValue((int)this->IsEnabled((vr::DriverId_t)args[0].asInt()));
        });
    }
}
RpcDriverManager::RpcDriverManager(RpcObjectId id) : RpcObject(id) {}
RpcDriverManager::~RpcDriverManager() {}

RpcClassEnum RpcDriverManager::GetRpcClassId() const {
    return RPCClassDriverManager;
}

uint32_t RpcDriverManager::GetDriverCount() const {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), RPCFunction_DriverManager_GetDriverCount).asInt();
    }
    else {
        return real_manager_->GetDriverCount();
    }
}

uint32_t RpcDriverManager::GetDriverName(vr::DriverId_t nDriver, char *pchValue, uint32_t unBufferSize) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverManager_GetDriverName, RpcValue((int)nDriver));
        std::string name = result.asString();
        if (pchValue && unBufferSize > 0) {
            memcpy(pchValue, name.c_str(), std::min((size_t)unBufferSize - 1, name.size()));
            pchValue[std::min((size_t)unBufferSize - 1, name.size())] = '\0';
        }
        return name.length() + 1;
    }
    else {
        return real_manager_->GetDriverName(nDriver, pchValue, unBufferSize);
    }
}

vr::DriverHandle_t RpcDriverManager::GetDriverHandle(const char *pchDriverName) {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), RPCFunction_DriverManager_GetDriverHandle, RpcValue(std::string(pchDriverName))).asInt();
    }
    else {
        return real_manager_->GetDriverHandle(pchDriverName);
    }
}

bool RpcDriverManager::IsEnabled(vr::DriverId_t nDriver) const {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), RPCFunction_DriverManager_IsEnabled, RpcValue((int)nDriver)).asInt();
    }
    else {
        return real_manager_->IsEnabled(nDriver);
    }
}
