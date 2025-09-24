#include "driver.hpp"
#include "vr_rpc_interfaces.h"

#include <string>
#include <memory>

static RpcServerTrackedDeviceProvider* g_pProviderProxy = nullptr;

void RegisterClasses() {
    RpcSystem::RegisterClass<RpcServerTrackedDeviceProvider>();
    RpcSystem::RegisterClass<ClientContextManager>();
    RpcSystem::RegisterClass<RpcDriverHost>();
    RpcSystem::RegisterClass<RpcDriverLog>();
    RpcSystem::RegisterClass<RpcSettings>();
    RpcSystem::RegisterClass<RpcTrackedDeviceServerDriver>();
    RpcSystem::RegisterClass<RpcDriverInput>();
    RpcSystem::RegisterClass<RpcDriverManager>();
    RpcSystem::RegisterClass<RpcProperties>();
    RpcSystem::RegisterClass<RpcResources>();
    RpcSystem::RegisterClass<RpcDisplayComponent>();
    RpcSystem::RegisterClass<RpcCameraComponent>();
}

HMD_DLL_EXPORT
void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode) {
    // Initialize on first call
    static bool s_initialized = false;
    if (!s_initialized) {
        s_initialized = true;
        RpcSystem::Initialize("ignition_pipe");
        RegisterClasses();
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
            RpcValue provider_val = RpcSystem::Call(vr::IServerTrackedDeviceProvider_Version);
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
