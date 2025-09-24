#include "rpc_core.h"
#include "vr_rpc_interfaces.h"

#include <openvr.hpp>
#include <windows.h>
#include <combaseapi.h>
#include <string>

void *(*pfnHmdDriverFactory)(const char *pInterfaceName, int *pReturnCode) = nullptr;

// Global pointer to the provider so we can register it.
RpcServerTrackedDeviceProvider* g_pRpcProvider = nullptr;

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

#define HARDCODED_DRIVER_PATH 1

int main(int argc, char *argv[])
{
  printf("Starting ignition_server...\n");

  if (FAILED(CoInitializeEx(NULL, COINIT_MULTITHREADED))) {
      printf("Failed to initialize COM.\n");
      return -1;
  }

  RpcSystem::Initialize("ignition_pipe");
  RegisterClasses();

  HMODULE hModule;
#ifdef HARDCODED_DRIVER_PATH
  hModule = LoadLibraryW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\PlayStation VR2 App\\SteamVR_Plug-In\\bin\\win64\\driver_playstation_vr2tk.dll");
#else
  if (argc < 2)
  {
    printf("usage: ignition_server.exe <driver path>\n");
    return 1;
  }
  hModule = LoadLibraryA(argv[1]);
#endif

  if (!hModule)
  {
    printf("Failed to load driver. LastError = %i\n", GetLastError());
    return -1;
  }

  // Set working directory
  //SetCurrentDirectoryA("C:\\Program Files (x86)\\Steam\\steamapps\\common\\PlayStation VR2 App\\SteamVR_Plug-In\\bin\\win64");

  pfnHmdDriverFactory = decltype(pfnHmdDriverFactory)(GetProcAddress(hModule, "HmdDriverFactory"));
  if (!pfnHmdDriverFactory)
  {
      printf("Failed to find HmdDriverFactory in driver DLL.\n");
      return -1;
  }

  int returnCode = vr::VRInitError_None;
  auto *pRealDeviceProvider =
      static_cast<vr::IServerTrackedDeviceProvider *>(pfnHmdDriverFactory(vr::IServerTrackedDeviceProvider_Version, &returnCode));
  
  if (returnCode != vr::VRInitError_None || !pRealDeviceProvider)
  {
    printf("HmdDriverFactory failed to get device provider interface. Code: %d\n", returnCode);
    return -1;
  }

  // Loop to allow server to be reused after a client disconnects.
  while (true) {
      RpcSystem::StartServer();

      // Create the RPC wrapper for the real provider.
      g_pRpcProvider = new RpcServerTrackedDeviceProvider(pRealDeviceProvider);
      
      // Register a function that the client can call to get a proxy to our provider.
      RpcSystem::RegisterFunction(vr::IServerTrackedDeviceProvider_Version, [](const auto& args) {
        auto val = RpcValue(g_pRpcProvider);
        return val;
      });
      
      printf("Ignition server is running. Waiting for client driver to connect...\n");

      while (RpcSystem::IsConnected())
      {
        // The listen thread handles everything. We can do other work here if needed.
        Sleep(1000);
      }

      printf("Client disconnected. Shutting down for this session.\n");
      delete g_pRpcProvider;
      g_pRpcProvider = nullptr;
      RpcSystem::Shutdown();
      printf("Restarting server to wait for new client...\n");
  }

  printf("Client disconnected. Shutting down.\n");
  delete g_pRpcProvider;
  RpcSystem::Shutdown();

  CoUninitialize();

  return 0;
}
