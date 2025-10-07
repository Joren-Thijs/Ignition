#pragma once

#include <cstdint>

enum RpcClassEnum : uint32_t
{
    RPCClassInvalid = 0,
    RPCClassStaticFunctions,
    RPCClassClientContextManager,
    RPCClassServerTrackedDeviceProvider,
    RPCClassDriverHost,
    RPCClassDriverLog,
    RPCClassSettings,
    RPCClassTrackedDeviceServerDriver,
    RPCClassDisplayComponent,
    RPCClassCameraComponent,
    RPCClassDriverInput,
    RPCClassDriverManager,
    RPCClassProperties,
    RPCClassResources,
};

enum RpcFunctionEnum : uint32_t
{
    // Static getter for IServerTrackedDeviceProvider
    RPCFunction_Get_ServerTrackedDeviceProvider,

    // IServerTrackedDeviceProvider
    RPCFunction_ServerTrackedDeviceProvider_Init,
    RPCFunction_ServerTrackedDeviceProvider_Cleanup,
    RPCFunction_ServerTrackedDeviceProvider_GetInterfaceVersions,
    RPCFunction_ServerTrackedDeviceProvider_RunFrame,
    RPCFunction_ServerTrackedDeviceProvider_ShouldBlockStandbyMode,
    RPCFunction_ServerTrackedDeviceProvider_EnterStandby,
    RPCFunction_ServerTrackedDeviceProvider_LeaveStandby,
    
    // IVRServerDriverHost
    RPCFunction_DriverHost_TrackedDeviceAdded,
    RPCFunction_DriverHost_TrackedDevicePoseUpdated,
    RPCFunction_DriverHost_VendorSpecificEvent,
    RPCFunction_DriverHost_VsyncEvent,
    RPCFunction_DriverHost_IsExiting,
    RPCFunction_DriverHost_RequestRestart,
    RPCFunction_DriverHost_SetRecommendedRenderTargetSize,
    RPCFunction_DriverHost_PollNextEvent,
    RPCFunction_DriverHost_GetRawTrackedDevicePoses,
    RPCFunction_DriverHost_GetFrameTimings,
    RPCFunction_DriverHost_SetDisplayEyeToHead,
    RPCFunction_DriverHost_SetDisplayProjectionRaw,

    // IVRDriverContext
    RPCFunction_ClientContextManager_GetGenericInterface,
    RPCFunction_ClientContextManager_GetDriverHandleContext,

    // IVRDriverLog
    RPCFunction_DriverLog_Log,

    // IVRSettings
    RPCFunction_Settings_GetSettingsErrorNameFromEnum,
    RPCFunction_Settings_SetBool,
    RPCFunction_Settings_SetInt32,
    RPCFunction_Settings_SetFloat,
    RPCFunction_Settings_SetString,
    RPCFunction_Settings_GetBool,
    RPCFunction_Settings_GetInt32,
    RPCFunction_Settings_GetFloat,
    RPCFunction_Settings_GetString,
    RPCFunction_Settings_RemoveSection,
    RPCFunction_Settings_RemoveKeyInSection,

    // ITrackedDeviceServerDriver
    RPCFunction_TrackedDeviceServerDriver_Activate,
    RPCFunction_TrackedDeviceServerDriver_Deactivate,
    RPCFunction_TrackedDeviceServerDriver_EnterStandby,
    RPCFunction_TrackedDeviceServerDriver_GetComponent,
    RPCFunction_TrackedDeviceServerDriver_DebugRequest,
    RPCFunction_TrackedDeviceServerDriver_GetPose,

    // IVRDisplayComponent
    RPCFunction_DisplayComponent_GetWindowBounds,
    RPCFunction_DisplayComponent_IsDisplayOnDesktop,
    RPCFunction_DisplayComponent_IsDisplayRealDisplay,
    RPCFunction_DisplayComponent_GetRecommendedRenderTargetSize,
    RPCFunction_DisplayComponent_GetEyeOutputViewport,
    RPCFunction_DisplayComponent_GetProjectionRaw,
    RPCFunction_DisplayComponent_ComputeDistortion,
    RPCFunction_DisplayComponent_ComputeInverseDistortion,

    // IVRCameraComponent
    RPCFunction_CameraComponent_GetProjectionRaw,
    RPCFunction_CameraComponent_GetCameraFrameDimensions,
    RPCFunction_CameraComponent_GetCameraFrameBufferingRequirements,
    RPCFunction_CameraComponent_StartVideoStream,
    RPCFunction_CameraComponent_StopVideoStream,
    RPCFunction_CameraComponent_IsVideoStreamActive,
    RPCFunction_CameraComponent_SetCameraFrameBuffering,
    RPCFunction_CameraComponent_SetCameraVideoStreamFormat,
    RPCFunction_CameraComponent_GetCameraVideoStreamFormat,
    RPCFunction_CameraComponent_GetVideoStreamFrame,
    RPCFunction_CameraComponent_ReleaseVideoStreamFrame,
    RPCFunction_CameraComponent_SetAutoExposure,
    RPCFunction_CameraComponent_PauseVideoStream,
    RPCFunction_CameraComponent_ResumeVideoStream,
    RPCFunction_CameraComponent_GetCameraDistortion,
    RPCFunction_CameraComponent_GetCameraProjection,
    RPCFunction_CameraComponent_SetFrameRate,
    RPCFunction_CameraComponent_SetCameraVideoSinkCallback,
    RPCFunction_CameraComponent_GetCameraCompatibilityMode,
    RPCFunction_CameraComponent_SetCameraCompatibilityMode,
    RPCFunction_CameraComponent_GetCameraFrameBounds,
    RPCFunction_CameraComponent_GetCameraIntrinsics,

    // IVRDriverInput
    RPCFunction_DriverInput_CreateBooleanComponent,
    RPCFunction_DriverInput_UpdateBooleanComponent,
    RPCFunction_DriverInput_CreateScalarComponent,
    RPCFunction_DriverInput_UpdateScalarComponent,
    RPCFunction_DriverInput_CreateHapticComponent,
    RPCFunction_DriverInput_CreateSkeletonComponent,
    RPCFunction_DriverInput_UpdateSkeletonComponent,
    RPCFunction_DriverInput_CreatePoseComponent,
    RPCFunction_DriverInput_UpdatePoseComponent,
    RPCFunction_DriverInput_CreateEyeTrackingComponent,
    RPCFunction_DriverInput_UpdateEyeTrackingComponent,

    // IVRDriverManager
    RPCFunction_DriverManager_GetDriverCount,
    RPCFunction_DriverManager_GetDriverName,
    RPCFunction_DriverManager_GetDriverHandle,
    RPCFunction_DriverManager_IsEnabled,

    // IVRProperties
    RPCFunction_Properties_ReadPropertyBatch,
    RPCFunction_Properties_WritePropertyBatch,
    RPCFunction_Properties_GetPropErrorNameFromEnum,
    RPCFunction_Properties_TrackedDeviceToPropertyContainer,

    // IVRResources
    RPCFunction_Resources_LoadSharedResource,
    RPCFunction_Resources_GetResourceFullPath,
    RPCFunction_Resources_GetResourceSize,
    RPCFunction_Resources_GetResourceData
};