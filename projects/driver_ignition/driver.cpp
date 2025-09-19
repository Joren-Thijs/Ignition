#include "driver.hpp"
#include "server_tracked_devices_provider.hpp"

#include <string>

HMD_DLL_EXPORT
void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode) {
    const std::string interfaceName(pInterfaceName);

    std::shared_ptr<ipc::IpcClient> client;
    ipc::InterfaceHandle handle = client->GetInterface(pInterfaceName, pReturnCode);

    // Server doesn't handle this interface
    if (*pReturnCode != vr::VRInitError_None) {
        return nullptr;
    }

    if (interfaceName == vr::IServerTrackedDeviceProvider_Version) {
        // return <global for server driver provider>;
    }
    if (interfaceName == vr::IVRWatchdogProvider_Version) {
        // return <global for watchdog driver>;
    }

    if (pReturnCode) {
        *pReturnCode = vr::VRInitError_Init_InterfaceNotFound;
    }

    return nullptr;
}
