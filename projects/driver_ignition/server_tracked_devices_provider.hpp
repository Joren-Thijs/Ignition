#pragma once

#include <openvr.hpp>
#include "ipc.hpp"

class IgnitionServerTrackedDeviceProvider : public vr::IServerTrackedDeviceProvider
{
private:
    std::shared_ptr<ipc::InterfaceHandle> handle;

public:
    IgnitionServerTrackedDeviceProvider(std::shared_ptr<ipc::InterfaceHandle> handle);

    virtual ~IgnitionServerTrackedDeviceProvider() = default;

    vr::EVRInitError Init(vr::IVRDriverContext *pDriverContext) override;

    void Cleanup() override;

    const char *const *GetInterfaceVersions() override;

    void RunFrame() override;

    bool ShouldBlockStandbyMode() override;

    void EnterStandby() override;

    void LeaveStandby() override;
};
