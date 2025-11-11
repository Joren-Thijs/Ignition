#include "driver.hpp"
#include "rpc_interfaces.h"

#include <fstream>
#include <string>
#include <memory>

#include <json.hpp>

#ifdef _WIN32
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <sys/types.h>
#include <dlfcn.h>
#include <unistd.h>
#include <linux/limits.h>
#include <vector>
#endif

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

std::string GetProcessId()
{
#ifdef _WIN32
    return std::to_string(GetCurrentProcessId());
#else
    return std::to_string(getpid());
#endif
}

std::string GetDriverDirectory() {
#ifdef _WIN32
    HMODULE hModule = NULL;
    GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
        (LPCSTR)&HmdDriverFactory,
        &hModule);

    char path[MAX_PATH];
    GetModuleFileNameA(hModule, path, sizeof(path));
    PathRemoveFileSpecA(path);
    return std::string(path);
#else
    Dl_info info;
    if (dladdr((void*)HmdDriverFactory, &info) != 0)
    {
        std::string path = info.dli_fname;
        return path.substr(0, path.find_last_of("/\\"));
    }

    // Fallback in case dladdr fails
    return ".";
#endif
}

void LaunchServer() {
    std::string driver_dir = GetDriverDirectory();
    std::string pid_str = GetProcessId();
    std::string config_path = driver_dir + "/ignition.json";

    std::ifstream config_file(config_path);
    if (!config_file.is_open()) {
        // Log: "Could not open ignition.json"
        return;
    }

    nlohmann::json config;
    try {
        config_file >> config;
    } catch (const std::exception&) {
        // Log: "Failed to parse ignition.json"
        return;
    }

    std::string server_path;
    if (config.contains("server_path")) {
        server_path = config["server_path"];
    } else {
        // Log: "ignition.json is missing 'server_path'"
        return;
    }

#ifdef _WIN32
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    std::string command_line = "\"" + server_path + "\" " + pid_str;
    std::vector<char> command_line_vec(command_line.begin(), command_line.end());
    command_line_vec.push_back('\0');

    // Start the child process.
    if (!CreateProcessA(NULL,   // No module name (use command line)
        command_line_vec.data(),        // Command line
        NULL,           // Process handle not inheritable
        NULL,           // Thread handle not inheritable
        FALSE,          // Set handle inheritance to FALSE
        CREATE_NO_WINDOW, // Creation flags
        NULL,           // Use parent's environment block
        NULL,           // Use parent's starting directory
        &si,            // Pointer to STARTUPINFO structure
        &pi)           // Pointer to PROCESS_INFORMATION structure
        )
    {
        // Log: "CreateProcess failed"
        return;
    }

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else // Linux
    std::string wine_path = "wine";
    if (config.contains("wine_path")) {
        wine_path = config["wine_path"];
    }

    pid_t pid = fork();
    if (pid == 0) { // Child process
        // We are in the child. Execute the server.
        execl(wine_path.c_str(), wine_path.c_str(), server_path.c_str(), pid_str.c_str(), (char*)NULL);
        // If execl returns, it must have failed.
        exit(1);
    }
    // Parent process continues.
#endif
}

HMD_DLL_EXPORT
void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode) {
    // Initialize on first call
    static bool s_initialized = false;
    if (!s_initialized) {
        s_initialized = true;
        std::string pipe_name = "ignition_pipe_" + GetProcessId();
        RpcSystem::Initialize(pipe_name);
        RegisterRPCClasses();
    }

    // Attempt to connect if not already connected
    try {
        LaunchServer();
        RpcSystem::StartServer();
    } catch(const std::exception& ) {}

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
