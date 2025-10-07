#pragma once

#include <cstdint>

enum RpcClassEnum : uint32_t
{
    Class_Invalid = 0,
    Class_StaticFunctions,
    Class_ClientContextManager,
    Class_ServerTrackedDeviceProvider,
    Class_DriverHost,
    Class_DriverLog,
    Class_Settings,
    Class_TrackedDeviceServerDriver,
    Class_DisplayComponent,
    Class_CameraComponent,
    Class_DriverInput,
    Class_DriverManager,
    Class_Properties,
    Class_Resources,
};