#include "driver.hpp"
#include "rpc_interfaces.h"
#include "config.h"
#include <string>

#include <json.hpp>

#ifndef __linux__
#include <windows.h>
#include <shlwapi.h>
#pragma comment(lib, "shlwapi.lib")
#else
#include <sys/types.h>
#include <dlfcn.h>
#include <unistd.h>
#include <linux/limits.h>
#include <vector>
#include <sys/prctl.h>
#include <signal.h>
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
#ifndef __linux__
    return std::to_string(GetCurrentProcessId());
#else
    return std::to_string(getpid());
#endif
}

std::string GetDriverDirectory() {
#ifndef __linux__
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

void LaunchServer(const IgnitionConfig& config) {
    std::string driver_dir = GetDriverDirectory();
    std::string pid_str = GetProcessId();

#ifndef __linux__
    STARTUPINFOA si;
    PROCESS_INFORMATION pi;

    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));

    // Build path with driver_dir and config.server_exe
    char full_path[MAX_PATH];
    PathCombineA(full_path, driver_dir.c_str(), config.server_exe.c_str());

    std::string command_line = "\"" + std::string(full_path) + "\" " + pid_str;
    std::vector<char> command_line_vec(command_line.begin(), command_line.end());
    command_line_vec.push_back('\0');

    // Start the child process.
    if (!CreateProcessA(NULL,  // No module name (use command line)
        command_line_vec.data(),   // Command line
        NULL,                // Process handle not inheritable
        NULL,                 // Thread handle not inheritable
        FALSE,                   // Set handle inheritance to FALSE
        CREATE_NO_WINDOW,        // Creation flags
        NULL,                      // Use parent's environment block
        driver_dir.c_str(),   // Use driver binary directory
        &si,                       // Pointer to STARTUPINFO structure
        &pi)                // Pointer to PROCESS_INFORMATION structure
        )
    {
        std::cerr << "CreateProcess failed (" << GetLastError() << ")" << std::endl;
        return;
    }

    // Close process and thread handles.
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
#else
    std::string wine_path = config.wine_cmd[0];

    pid_t pid = fork();
    if (pid == 0) {
        // We are in the child. Execute the server.

        // First, change working directory to where driver is located
        if (chdir(driver_dir.c_str()) == -1) {
            std::cerr << "Failed to change working directory" << std::endl;
            exit(1);
        }

        // Ensure the child process is killed when the parent exits.
        prctl(PR_SET_PDEATHSIG, SIGHUP);
        
        // Execute
        std::vector<const char*> args;
        for (const auto& arg : config.wine_cmd) {
            args.push_back(arg.c_str());
        }
        args.push_back(config.server_exe.c_str());
        args.push_back(pid_str.c_str());

        args.push_back(NULL); // Null-terminate the argument list

        int r = execvp(wine_path.c_str(), const_cast<char* const*>(args.data()));

        // If execvp returns, it must have failed.
        perror("execvp");
        exit(1);
    }    
#endif
}

HMD_DLL_EXPORT
void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode) {
    // Initialize on first call
    static bool s_initialized = false;

    if (!s_initialized) {
        s_initialized = true;

        std::string driver_dir = GetDriverDirectory();
        std::string config_path = driver_dir + "/ignition.json";
        IgnitionConfig config;
        if (!ParseConfig(config_path, config)) {
            // Error already printed in ParseConfig
            return nullptr;
        }

        std::string pipe_name = "ignition_ipc_" + GetProcessId() + "_" + config.driver_dll;
        RpcSystem::Initialize(pipe_name);
        RegisterRPCClasses();

        // Create IPC, and then launch server (which will connect to the IPC we created)
        RpcSystem::CreateIPC();
        LaunchServer(config);
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
