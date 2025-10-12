#include "rpc_core.h"
#include "rpc_interfaces.h"

#include <combaseapi.h>
#include <iostream>
#include <openvr.hpp>
#include <string>
#include <windows.h>

void *(*pfnHmdDriverFactory)(const char *pInterfaceName, int *pReturnCode) = nullptr;

// Global pointer to the provider so we can register it.
RpcServerTrackedDeviceProvider* g_pRpcProvider = nullptr;

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

int main(int argc, char *argv[]) {
    std::cout << "Ignition server starting..." << std::endl;

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        printf("Failed to initialize COM.\n");
        return -1;
    }

    RpcSystem::Initialize("ignition_pipe");
    RegisterRPCClasses();

    HMODULE hModule;
#ifdef HARDCODED_DRIVER_PATH
    hModule = LoadLibraryW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\PlayStation VR2 App\\SteamVR_Plug-In\\bin\\win64\\driver_playstation_vr2tk.dll");
#else
    if (argc < 2) {
        std::cout << "Usage: ignition_server <path to driver DLL>" << std::endl;
        return 1;
    }
    hModule = LoadLibraryA(argv[1]);
#endif

    if (!hModule) {
        std::cout << "Failed to load driver DLL. Error: " << GetLastError() << std::endl;
        return -1;
    }

    pfnHmdDriverFactory = decltype(pfnHmdDriverFactory)(GetProcAddress(hModule, "HmdDriverFactory"));
    if (!pfnHmdDriverFactory) {
        std::cout << "Failed to get HmdDriverFactory address. Error: " << GetLastError() << std::endl;
        return -1;
    }

    int returnCode = vr::VRInitError_None;
    auto *pRealDeviceProvider = static_cast<vr::IServerTrackedDeviceProvider *>(
        pfnHmdDriverFactory(vr::IServerTrackedDeviceProvider_Version, &returnCode));

    if (returnCode != vr::VRInitError_None || !pRealDeviceProvider) {
        std::cout << "HmdDriverFactory failed to get IServerTrackedDeviceProvider. "
                     "Error: "
                  << returnCode << std::endl;
        return -1;
    }

    RpcSystem::StartServer();

    // Create the RPC wrapper for the real provider.
    g_pRpcProvider = new RpcServerTrackedDeviceProvider(pRealDeviceProvider);

    // Register a function that the client can call to get a proxy to our
    // provider. We'll register it on a special "static" object with ID 0.
    RpcSystem::RegisterFunction(RPCFunction_Get_ServerTrackedDeviceProvider, [](const auto &args) {
        auto val = RpcValue(g_pRpcProvider);
        return val;
    });

    std::cout << "Ignition server running. Waiting for client to connect..." << std::endl;

    while (RpcSystem::IsConnected()) {
        // The listen thread handles everything. We can do other work here if
        // needed.
        Sleep(1000);
    }

    std::cout << "Client disconnected, shutting down." << std::endl;

    delete g_pRpcProvider;
    RpcSystem::Shutdown();

    CoUninitialize();

    return 0;
}
