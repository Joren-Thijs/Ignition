#include "driver.hpp"
#include "vr_rpc_interfaces.h"

#include <string>
#include <memory>

static RpcServerTrackedDeviceProvider* g_pProviderProxy = nullptr;

void RegisterRPCClasses() {
    RpcSystem::RegisterRPCClass<RpcServerTrackedDeviceProvider>();
    RpcSystem::RegisterRPCClass<ClientContextManager>();
    RpcSystem::RegisterRPCClass<RpcDriverHost>();
    RpcSystem::RegisterRPCClass<RpcDriverLog>();
    RpcSystem::RegisterRPCClass<RpcSettings>();
    RpcSystem::RegisterRPCClass<RpcTrackedDeviceServerDriver>();
    RpcSystem::RegisterRPCClass<RpcDriverInput>();
    RpcSystem::RegisterRPCClass<RpcDriverManager>();
    RpcSystem::RegisterRPCClass<RpcProperties>();
    RpcSystem::RegisterRPCClass<RpcResources>();
    RpcSystem::RegisterRPCClass<RpcDisplayComponent>();
    RpcSystem::RegisterRPCClass<RpcCameraComponent>();
}

HMD_DLL_EXPORT
void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode) {
    // Initialize on first call
    static bool s_initialized = false;
    if (!s_initialized) {
        s_initialized = true;
        RpcSystem::Initialize("ignition_pipe");
        RegisterRPCClasses();
    }

    // Attempt to connect if not already connected
    if (!RpcSystem::IsConnected() && !RpcSystem::ConnectToServer()) {
        // Don't permanently fail here, SteamVR might retry HmdDriverFactory.
        // Only fail if the connection attempt itself throws an exception.
        try {
        } catch(const std::exception& e) {
            // Log this error if you have a logging mechanism
            if (pReturnCode) *pReturnCode = vr::VRInitError_IPC_Failed;
            return nullptr;
        }
    }

    if (strcmp(pInterfaceName, vr::IServerTrackedDeviceProvider_Version) == 0) {
        if (g_pProviderProxy) {
            if (pReturnCode) *pReturnCode = vr::VRInitError_None;
            return g_pProviderProxy;
        }

        try {
            RpcValue provider_val = RpcSystem::Call(RPCFunction_Get_ServerTrackedDeviceProvider, RpcValue());
            if (provider_val.isObject()) {
                 if (pReturnCode) *pReturnCode = vr::VRInitError_None;
                 g_pProviderProxy = static_cast<RpcServerTrackedDeviceProvider*>(provider_val.asObject());
                 return g_pProviderProxy;
            }
        } catch (const std::exception& e) {
            // Log this error
            if (pReturnCode) *pReturnCode = vr::VRInitError_IPC_Failed;
            return nullptr;
        }
    }

    if (pReturnCode) {
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    }

    return nullptr;
}
