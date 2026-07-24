#include "rpc_core.h"
#include "wine_utils.h"
#include "rpc_interfaces.h"
#include "config.h"
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include <openvr_driver.h>

#ifdef _WIN32
#include <combaseapi.h>
#include <shlwapi.h>
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
    RpcSystem::RegisterRPCClass<RpcPaths>();
    RpcSystem::RegisterRPCClass<RpcBlockQueue>();
}

int main(int argc, char *argv[]) {
    std::cout << "Ignition server starting..." << std::endl;

    if (argc < 3) {
        std::cout << "Usage: ignition_server <connection string> <config path>" << std::endl;
        return 1;
    }

    std::string connection_str = argv[1];
    std::string config_path = argv[2];

#ifdef _WIN32
    if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
        printf("Failed to initialize COM.\n");
        return -1;
    }
    
    if (IsRunningInWine()) {
        // We are running in wine, so translate the unix path to a windows path.
        config_path = WineGetDosFileName(config_path);
    }
#endif
    
    // Read config to find the driver DLL
    IgnitionConfig config;
    if (!ParseConfig(config_path, config)) {
        // Error already printed in ParseConfig
        return -1;
    }

    if (config.wait_for_debugger) {
#ifdef _WIN32
        std::cout << "Waiting for debugger to attach..." << std::endl;
        while (!IsDebuggerPresent()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
#else
        std::cout << "Waiting 5 seconds..." << std::endl;
        std::this_thread::sleep_for(std::chrono::seconds(5));
#endif
    }

    std::string pipe_name = "ignition_ipc_" + connection_str;
    RpcSystem::Initialize(pipe_name);
    RegisterRPCClasses();

#ifdef _WIN32
    // Not having a full path leads to undefined behavior with LOAD_WITH_ALTERED_SEARCH_PATH.
	// So, GetFullPathNameA will be used to ensure we have a full path.
	char full_path[MAX_PATH];
	if (GetFullPathNameA(config.driver_dll.c_str(), MAX_PATH, full_path, NULL) == 0) {
		std::cout << "Failed to get full path for driver DLL." << std::endl;
		return -1;
	}
	
	HMODULE hModule = LoadLibraryExA(full_path, NULL, LOAD_WITH_ALTERED_SEARCH_PATH);

    if (!hModule) {
        std::cout << "Failed to load driver DLL. Error: " << GetLastError() << std::endl;
        return -1;
    }

    pfnHmdDriverFactory = decltype(pfnHmdDriverFactory)(GetProcAddress(hModule, "HmdDriverFactory"));
    if (!pfnHmdDriverFactory) {
        std::cout << "Failed to get HmdDriverFactory address. Error: " << GetLastError() << std::endl;
        return -1;
    }
#else
    void* handle = dlopen(config.driver_dll.c_str(), RTLD_LAZY);
    if (!handle) {
        std::cerr << "Failed to load driver DLL: " << dlerror() << std::endl;
        return -1;
    }

    pfnHmdDriverFactory = reinterpret_cast<decltype(pfnHmdDriverFactory)>(dlsym(handle, "HmdDriverFactory"));
    if (!pfnHmdDriverFactory) {
        std::cerr << "Failed to get HmdDriverFactory address: " << dlerror() << std::endl;
        dlclose(handle);
        return -1;
    }
#endif

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

    std::cout << "Ignition server running." << std::endl;

    while (true) {
        if (!RpcSystem::IsAlive()) {
            break;
        }
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    std::cout << "Client disconnected, shutting down." << std::endl;
    RpcSystem::Shutdown();

    delete g_pRpcProvider;

#ifdef _WIN32
    FreeLibrary(hModule);
#else
    dlclose(handle);
#endif

#ifdef _WIN32
    CoUninitialize();
#endif

    return 0;
}

#if defined(_WIN32) && !defined(__WINE__)
// Redirect to the actual main.
// WinMain is used that so we can build as a windowed app
// to not show a console when running under Proton.
int APIENTRY WinMain(HINSTANCE hInst, HINSTANCE hInstPrev, PSTR cmdline, int cmdshow) {
    return main(__argc, __argv);
}
#endif