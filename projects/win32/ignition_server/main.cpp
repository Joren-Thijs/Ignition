#include "rpc_core.h"
#include "rpc_interfaces.h"

#include <fstream>
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

    Sleep(1000);

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

    std::string pipe_name = "ignition_pipe_" + pid_str;
    RpcSystem::Initialize(pipe_name);
    RegisterRPCClasses();

    // Read config to find the driver DLL
    std::string config_path = "./ignition.json";
    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        std::cout << "Could not open ignition.json" << std::endl;
        return -1;
    }

    nlohmann::json config;
    try {
        config_file >> config;
    } catch (const std::exception& e) {
        std::cout << "Failed to parse ignition.json: " << e.what() << std::endl;
        return -1;
    }

    std::string driver_path;
    if (config.contains("driver_path")) {
        driver_path = config["driver_path"];
    } else {
        std::cout << "ignition.json is missing 'driver_path'" << std::endl;
        return -1;
    }

    HMODULE hModule = LoadLibraryA(driver_path.c_str());

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

    RpcSystem::ConnectToServer();

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
        std::cout << "Could not open driver process. Quitting." << std::endl;
    }

    std::cout << "Client disconnected, shutting down." << std::endl;

    delete g_pRpcProvider;

    CoUninitialize();

    return 0;
}
