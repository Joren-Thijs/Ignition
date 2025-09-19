#include "server_tracked_devices_provider.hpp"

IgnitionServerTrackedDeviceProvider::IgnitionServerTrackedDeviceProvider(
    std::shared_ptr<ipc::InterfaceHandle> handle) {
    this->handle = handle;
}

vr::EVRInitError IgnitionServerTrackedDeviceProvider::Init(vr::IVRDriverContext *pDriverContext) {
    VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext)

    auto ipc_error = static_cast<vr::EVRInitError>(this->handle->MakeCallError({
        .type = ipc::Command::Type::IServerTrackedDeviceProvider_Init,
        .payload = {.IServerTrackedDeviceProvider_Init = {}},
    }));

    return ipc_error;
}

const char *const *IgnitionServerTrackedDeviceProvider::GetInterfaceVersions() {
    // No IPC call made, we always emulate the latest interface versions
    return vr::k_InterfaceVersions;
}

bool IgnitionServerTrackedDeviceProvider::ShouldBlockStandbyMode() {
    bool should_block = this->handle->MakeCallBool({
        .type = ipc::Command::Type::IServerTrackedDeviceProvider_ShouldBlockStandbyMode,
        .payload = {.IServerTrackedDeviceProvider_ShouldBlockStandbyMode = {}},
    });

    return should_block;
}
