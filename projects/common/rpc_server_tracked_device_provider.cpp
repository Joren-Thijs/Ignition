#include "rpc_interfaces.h"
#include <vector>

// --- RpcServerTrackedDeviceProvider ---

RpcServerTrackedDeviceProvider::RpcServerTrackedDeviceProvider(vr::IServerTrackedDeviceProvider* real) : RpcObject(), real_provider_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_ServerTrackedDeviceProvider_Init, [this](const auto& args) {
            // The server receives the proxy to the client's context manager
            ClientContextManager* context_proxy = static_cast<ClientContextManager*>(args[0].asObject());
            
            auto result = this->Init(context_proxy);
            
            return RpcValue((int)result);
        });
        
        this->RegisterFunction(RPCFunction_ServerTrackedDeviceProvider_ShouldBlockStandbyMode, [this](const auto& args) {
            return RpcValue((int)this->ShouldBlockStandbyMode());
        });
        
        this->RegisterFunction(RPCFunction_ServerTrackedDeviceProvider_Cleanup, [this](const auto& args) {
            this->Cleanup();
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_ServerTrackedDeviceProvider_RunFrame, [this](const auto& args) {
            this->RunFrame();
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_ServerTrackedDeviceProvider_EnterStandby, [this](const auto& args) {
            this->EnterStandby();
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_ServerTrackedDeviceProvider_LeaveStandby, [this](const auto& args) {
            this->LeaveStandby();
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_ServerTrackedDeviceProvider_GetInterfaceVersions, [this](const auto& args) {
            const char *const *versions = this->GetInterfaceVersions();
            if (!versions) {
                return RpcValue();
            }

            std::string concatenated_versions;
            for (int i = 0; versions[i] != nullptr; ++i) {
                concatenated_versions.append(versions[i]);
                concatenated_versions.push_back('\0'); // Use null char as separator
            }
            return RpcValue(concatenated_versions);
        });
    }
}

RpcServerTrackedDeviceProvider::RpcServerTrackedDeviceProvider(RpcObjectId id) : RpcObject(id), real_provider_(nullptr) {}

RpcServerTrackedDeviceProvider::~RpcServerTrackedDeviceProvider() {}

RpcClassEnum RpcServerTrackedDeviceProvider::GetRpcClassId() const {
    return RPCClassServerTrackedDeviceProvider;
}

vr::EVRInitError RpcServerTrackedDeviceProvider::Init(vr::IVRDriverContext *pDriverContext) {
    if (IsProxy()) {
        // Client side. The provided context is from SteamVR and is real.
        VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
        
        // Create a manager for this context so the server can get interfaces from it.
        auto* context_manager = new ClientContextManager(pDriverContext);

        // Call the server's Init, passing a proxy to our context manager
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_ServerTrackedDeviceProvider_Init, RpcValue(context_manager));
        return (vr::EVRInitError)result.asInt();
    }
    else {
        return real_provider_->Init(pDriverContext);
    }
}

void RpcServerTrackedDeviceProvider::Cleanup() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_ServerTrackedDeviceProvider_Cleanup);
    }
    else {
        real_provider_->Cleanup();
    }
}

const char *const *RpcServerTrackedDeviceProvider::GetInterfaceVersions() {
    if (IsProxy()) {
        // If we've already fetched them, return the cached version.
        if (!client_versions_ptrs_.empty()) {
            return client_versions_ptrs_.data();
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_ServerTrackedDeviceProvider_GetInterfaceVersions);
        if (result.isByteArray()) {
            const std::string& concatenated = result.asString();
            size_t start = 0;
            while (start < concatenated.length()) {
                size_t end = concatenated.find('\0', start);
                if (end == std::string::npos) {
                    break; // Should not happen if server implementation is correct
                }
                client_versions_storage_.push_back(concatenated.substr(start, end - start));
                start = end + 1;
            }

            // Build the array of C-string pointers
            for (const auto& str : client_versions_storage_) {
                client_versions_ptrs_.push_back(str.c_str());
            }
            client_versions_ptrs_.push_back(nullptr); // Null-terminate the array
            return client_versions_ptrs_.data();
        }
        return nullptr;
    } else {
        return real_provider_->GetInterfaceVersions();
    }
}

void RpcServerTrackedDeviceProvider::RunFrame() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_ServerTrackedDeviceProvider_RunFrame);
    }
    else {
        real_provider_->RunFrame();
    }
}

bool RpcServerTrackedDeviceProvider::ShouldBlockStandbyMode() {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), RPCFunction_ServerTrackedDeviceProvider_ShouldBlockStandbyMode).asInt();
    }
    else {
        return real_provider_->ShouldBlockStandbyMode();
    }
}

void RpcServerTrackedDeviceProvider::EnterStandby() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_ServerTrackedDeviceProvider_EnterStandby);
    }
    else {
        real_provider_->EnterStandby();
    }
}

void RpcServerTrackedDeviceProvider::LeaveStandby() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_ServerTrackedDeviceProvider_LeaveStandby);
    }
    else {
        real_provider_->LeaveStandby();
    }
}
