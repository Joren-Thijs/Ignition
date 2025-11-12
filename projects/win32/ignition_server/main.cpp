#include "rpc_core.h"
#include "rpc_interfaces.h"
#include "config.h"

#include <combaseapi.h>
#include <iostream>
#include <openvr.hpp>
#include <string>
#include <json.hpp>
#include <shlwapi.h>
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

    if (argc < 2) {
        std::cout << "Usage: ignition_server <driver process PID>" << std::endl;
        return 1;
    }

    std::string pid_str = argv[1];
    DWORD driver_pid = std::stoul(pid_str);

    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        printf("Failed to initialize COM.\n");
        return -1;
    }

    // Read config to find the driver DLL
    std::string config_path = "./ignition.json";
    IgnitionConfig config;
    if (!ParseConfig(config_path, config)) {
        // Error already printed in ParseConfig
        return -1;
    }

    std::string pipe_name = "ignition_ipc_" + pid_str + "_" + config.driver_dll;
    RpcSystem::Initialize(pipe_name);
    RegisterRPCClasses();

    HMODULE hModule = LoadLibraryA(config.driver_dll.c_str());

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

    RpcSystem::ConnectToExistingIPC();

    // Create the RPC wrapper for the real provider.
    g_pRpcProvider = new RpcServerTrackedDeviceProvider(pRealDeviceProvider);

    // Register a function that the client can call to get a proxy to our
    // provider. We'll register it on a special "static" object with ID 0.
    RpcSystem::RegisterFunction(RPCFunction_Get_ServerTrackedDeviceProvider, [](const auto &args) {
        auto val = RpcValue(g_pRpcProvider);
        return val;
    });

    std::cout << "Ignition server running. Waiting for client to connect..." << std::endl;

    HANDLE hDriverProcess = OpenProcess(SYNCHRONIZE, FALSE, driver_pid);

    if (hDriverProcess != NULL) {
        std::cout << "Monitoring driver process " << driver_pid << " for termination." << std::endl;
        WaitForSingleObject(hDriverProcess, INFINITE);
        std::cout << "Driver process terminated." << std::endl;
        CloseHandle(hDriverProcess);
    }
    else {
        std::cout << "Could not open driver process. We'll assume the driver will try to quit this process." << std::endl;

        while (true)
            Sleep(1000);
    }

    std::cout << "Client disconnected, shutting down." << std::endl;

    delete g_pRpcProvider;

    CoUninitialize();

    return 0;
}
