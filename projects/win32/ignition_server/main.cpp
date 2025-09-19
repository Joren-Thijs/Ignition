#include "driver_context.h"

#include <openvr_driver.h>
#include <windows.h>
#include <cstdio>

void *(*pfnHmdDriverFactory)(const char *pInterfaceName, int *pReturnCode) = nullptr;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    printf("usage: ignition_server.exe <driver path>");
    return 0;
  }

#ifdef HARDCODED_DRIVER_PATH
  HMODULE hModule = LoadLibraryW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\PlayStation VR2 App\\SteamVR_Plug-In\\bin\\win64\\driver_playstation_vr2.dll");
#else
  HMODULE hModule = LoadLibraryA(argv[1]);
#endif
  if (!hModule) {
    printf("Failed to load driver. LastError = %i\n", GetLastError());
    return -1;
  }

  pfnHmdDriverFactory = decltype(pfnHmdDriverFactory)(GetProcAddress(hModule, "HmdDriverFactory"));

  int returnCode = vr::VRInitError_None;
  vr::IServerTrackedDeviceProvider *pDeviceProvider =
    static_cast<vr::IServerTrackedDeviceProvider *>(pfnHmdDriverFactory(vr::IServerTrackedDeviceProvider_Version, &returnCode));
  if (returnCode != vr::VRInitError_None && !pDeviceProvider) {
    printf("Failed to get device provider interface.\n");
    return -1;
  }

  ignition::DriverContext driverContext = {};

  pDeviceProvider->Init(&driverContext);

  while (true) { Sleep(1); }

  return 0;
}
