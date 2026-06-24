#pragma once

#include "rpc_core.h"
#include <openvr_driver.h>
#include <vr_blockqueue.h>

// ************************************
// RPC wrapper for vr::IVRDriverContext
// ************************************
class ClientContextManager : public vr::IVRDriverContext, public RpcObject {
public:
    // Local constructor (client-side)
    ClientContextManager(vr::IVRDriverContext* real_context);
    // Proxy constructor (server-side)
    ClientContextManager(RpcObjectId id);

    ~ClientContextManager();

    /** IVRDriverContext **/
    void *GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError = nullptr) override;
    vr::DriverHandle_t GetDriverHandle() override;

    // RpcObject
    RpcClassEnum GetRpcClassId() const override;

private:
    vr::IVRDriverContext* real_context_ = nullptr; // Only valid on the client

    // To avoid creating multiple wrappers for the same real object
    std::map<std::string, RpcObject*> interface_cache_;
};


// ************************************************
// RPC wrapper for vr::IServerTrackedDeviceProvider
// ************************************************
class RpcServerTrackedDeviceProvider : public vr::IServerTrackedDeviceProvider, public RpcObject
{
public:
    // Server-side constructor (wraps the real driver's implementation)
    RpcServerTrackedDeviceProvider(vr::IServerTrackedDeviceProvider* real);
    // Client-side proxy constructor
    RpcServerTrackedDeviceProvider(RpcObjectId id);
    ~RpcServerTrackedDeviceProvider();

    RpcClassEnum GetRpcClassId() const override;

    // --- vr::IServerTrackedDeviceProvider implementation ---
    vr::EVRInitError Init(vr::IVRDriverContext *pDriverContext) override;
    void Cleanup() override;
    const char *const *GetInterfaceVersions() override;
    void RunFrame() override;
    bool ShouldBlockStandbyMode() override;
    void EnterStandby() override;
    void LeaveStandby() override;
private:
    vr::IServerTrackedDeviceProvider* real_provider_ = nullptr; // Only valid on the server

    // Client-side storage for interface versions
    std::vector<std::string> client_versions_storage_;
    std::vector<const char*> client_versions_ptrs_;
};

// ***************************************
// RPC wrapper for vr::IVRServerDriverHost
// ***************************************
class RpcDriverHost : public vr::IVRServerDriverHost, public RpcObject
{
public:
    // Client-side constructor (wraps the real interface from SteamVR)
    RpcDriverHost(vr::IVRServerDriverHost* real);
    // Server-side proxy constructor
    RpcDriverHost(RpcObjectId id);
    ~RpcDriverHost();

    RpcClassEnum GetRpcClassId() const override;

    // --- vr::IVRServerDriverHost implementation ---
    bool TrackedDeviceAdded(const char *pchDeviceSerialNumber, vr::ETrackedDeviceClass eDeviceClass, vr::ITrackedDeviceServerDriver *pDriver) override;
    void TrackedDevicePoseUpdated(uint32_t unWhichDevice, const vr::DriverPose_t &newPose, uint32_t unPoseStructSize) override;
    void VsyncEvent(double vsyncTimeOffsetSeconds) override;
    void VendorSpecificEvent(uint32_t unWhichDevice, vr::EVREventType eventType, const vr::VREvent_Data_t &eventData, double eventTimeOffset) override;
    bool IsExiting() override;
    bool PollNextEvent(vr::VREvent_t *pEvent, uint32_t uncbVREvent) override;
    void GetRawTrackedDevicePoses(float fPredictedSecondsFromNow, vr::TrackedDevicePose_t *pTrackedDevicePoseArray, uint32_t unTrackedDevicePoseArrayCount) override;
    void RequestRestart(const char *pchLocalizedReason, const char *pchExecutableToStart, const char *pchArguments, const char *pchWorkingDirectory) override;
    uint32_t GetFrameTimings(vr::Compositor_FrameTiming *pTiming, uint32_t nFrames) override;
    void SetDisplayEyeToHead(uint32_t unWhichDevice, const vr::HmdMatrix34_t &eyeToHeadLeft, const vr::HmdMatrix34_t &eyeToHeadRight) override;
    void SetDisplayProjectionRaw(uint32_t unWhichDevice, const vr::HmdRect2_t &eyeLeft, const vr::HmdRect2_t &eyeRight) override;
    void SetRecommendedRenderTargetSize(uint32_t unWhichDevice, uint32_t nWidth, uint32_t nHeight) override;

private:
    vr::IVRServerDriverHost* real_host_ = nullptr; // Only valid on the client
};

// ********************************
// RPC wrapper for vr::IVRDriverLog
// ********************************
class RpcDriverLog : public vr::IVRDriverLog, public RpcObject
{
public:
    RpcDriverLog(vr::IVRDriverLog* real);
    RpcDriverLog(RpcObjectId id);
    ~RpcDriverLog();

    RpcClassEnum GetRpcClassId() const override;

    void Log(const char *pchLogMessage) override;

private:
    vr::IVRDriverLog* real_log_ = nullptr;
};

// *******************************
// RPC wrapper for vr::IVRSettings
// *******************************
class RpcSettings : public vr::IVRSettings, public RpcObject
{
public:
    RpcSettings(vr::IVRSettings* real);
    RpcSettings(RpcObjectId id);
    ~RpcSettings();

    RpcClassEnum GetRpcClassId() const override;

    const char *GetSettingsErrorNameFromEnum(vr::EVRSettingsError eError) override;
    void SetBool(const char *pchSection, const char *pchSettingsKey, bool bValue, vr::EVRSettingsError *peError = nullptr) override;
    void SetInt32(const char *pchSection, const char *pchSettingsKey, int32_t nValue, vr::EVRSettingsError *peError = nullptr) override;
    void SetFloat(const char *pchSection, const char *pchSettingsKey, float flValue, vr::EVRSettingsError *peError = nullptr) override;
    void SetString(const char *pchSection, const char *pchSettingsKey, const char *pchValue, vr::EVRSettingsError *peError = nullptr) override;
    bool GetBool(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;
    int32_t GetInt32(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;
    float GetFloat(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;
    void GetString(const char *pchSection, const char *pchSettingsKey, VR_OUT_STRING() char *pchValue, uint32_t unValueLen, vr::EVRSettingsError *peError = nullptr) override;
    void RemoveSection(const char *pchSection, vr::EVRSettingsError *peError = nullptr) override;
    void RemoveKeyInSection(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;

private:
    vr::IVRSettings* real_settings_ = nullptr;
    std::map<vr::EVRSettingsError, std::string> error_name_cache_;
};

// **********************************************
// RPC wrapper for vr::ITrackedDeviceServerDriver
// **********************************************
class RpcTrackedDeviceServerDriver : public vr::ITrackedDeviceServerDriver, public RpcObject
{
public:
    // Server-side constructor (wraps the real driver's implementation)
    RpcTrackedDeviceServerDriver(vr::ITrackedDeviceServerDriver* real);
    // Client-side proxy constructor
    RpcTrackedDeviceServerDriver(RpcObjectId id);
    ~RpcTrackedDeviceServerDriver();

    RpcClassEnum GetRpcClassId() const override;

    // --- vr::ITrackedDeviceServerDriver implementation ---
    vr::EVRInitError Activate(uint32_t unObjectId) override;
    void Deactivate() override;
    void EnterStandby() override;
    void *GetComponent(const char *pchComponentNameAndVersion) override;
    void DebugRequest(const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize) override;
    vr::DriverPose_t GetPose() override;

private:
    vr::ITrackedDeviceServerDriver* real_driver_ = nullptr; // Only valid on the server
    std::map<std::string, RpcObject*> wrapped_components_; // Only valid on the server
};

// ***************************************
// RPC wrapper for vr::IVRDisplayComponent
// ***************************************
class RpcDisplayComponent : public vr::IVRDisplayComponent, public RpcObject
{
public:
    // Server-side constructor (wraps the real interface from the driver)
    RpcDisplayComponent(vr::IVRDisplayComponent* real);
    // Client-side proxy constructor
    RpcDisplayComponent(RpcObjectId id);
    ~RpcDisplayComponent();

    RpcClassEnum GetRpcClassId() const override;

    // --- vr::IVRDisplayComponent implementation ---
    void GetWindowBounds(int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) override;
    bool IsDisplayOnDesktop() override;
    bool IsDisplayRealDisplay() override;
    void GetRecommendedRenderTargetSize(uint32_t *pnWidth, uint32_t *pnHeight) override;
    void GetEyeOutputViewport(vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) override;
    void GetProjectionRaw(vr::EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom) override;
    vr::DistortionCoordinates_t ComputeDistortion(vr::EVREye eEye, float fU, float fV) override;
    bool ComputeInverseDistortion(vr::HmdVector2_t *pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV) override;

private:
    vr::IVRDisplayComponent* real_component_ = nullptr; // Only valid on the server
};

// **************************************
// RPC wrapper for vr::IVRCameraComponent
// **************************************
class RpcCameraComponent : public vr::IVRCameraComponent, public RpcObject
{
public:
    // Server-side constructor
    RpcCameraComponent(vr::IVRCameraComponent* real);
    // Client-side proxy constructor
    RpcCameraComponent(RpcObjectId id);
    ~RpcCameraComponent();

    RpcClassEnum GetRpcClassId() const override;

    // --- vr::IVRCameraComponent implementation ---
    bool GetCameraFrameDimensions(vr::ECameraVideoStreamFormat nVideoStreamFormat, uint32_t *pWidth, uint32_t *pHeight) override;
    bool GetCameraFrameBufferingRequirements(int *pDefaultFrameQueueSize, uint32_t *pFrameBufferDataSize) override;
    bool StartVideoStream() override;
    void StopVideoStream() override;
    bool IsVideoStreamActive(bool *pbPaused, float *pflElapsedTime) override;
    bool SetCameraFrameBuffering(int nFrameBufferCount, void **ppFrameBuffers, uint32_t nFrameBufferDataSize) override;
    bool SetCameraVideoStreamFormat(vr::ECameraVideoStreamFormat nVideoStreamFormat) override;
    vr::ECameraVideoStreamFormat GetCameraVideoStreamFormat() override;
    const vr::CameraVideoStreamFrame_t *GetVideoStreamFrame() override;
    void ReleaseVideoStreamFrame(const vr::CameraVideoStreamFrame_t *pFrameImage) override;
    bool SetAutoExposure(bool bEnable) override;
    bool PauseVideoStream() override;
    bool ResumeVideoStream() override;
    bool GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float *pflOutputU, float *pflOutputV) override;
    bool GetCameraProjection(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, float flZNear, float flZFar, vr::HmdMatrix44_t *pProjection) override;
    bool SetFrameRate(int nISPFrameRate, int nSensorFrameRate) override;
    bool SetCameraVideoSinkCallback(vr::ICameraVideoSinkCallback *pCameraVideoSinkCallback) override;
    bool GetCameraCompatibilityMode(vr::ECameraCompatibilityMode *pCameraCompatibilityMode) override;
    bool SetCameraCompatibilityMode(vr::ECameraCompatibilityMode nCameraCompatibilityMode) override;
    bool GetCameraFrameBounds(vr::EVRTrackedCameraFrameType eFrameType, uint32_t *pLeft, uint32_t *pTop, uint32_t *pWidth, uint32_t *pHeight) override;
    bool GetCameraIntrinsics(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, vr::HmdVector2_t *pFocalLength, vr::HmdVector2_t *pCenter, vr::EVRDistortionFunctionType *peDistortionType, double rCoefficients[vr::k_unMaxDistortionFunctionParameters]) override;

private:
    vr::IVRCameraComponent* real_component_ = nullptr; // Only valid on the server
};

// **********************************
// RPC wrapper for vr::IVRDriverInput
// **********************************
class RpcDriverInput : public vr::IVRDriverInput, public RpcObject
{
public:
    RpcDriverInput(vr::IVRDriverInput* real);
    RpcDriverInput(RpcObjectId id);
    ~RpcDriverInput();

    RpcClassEnum GetRpcClassId() const override;

    vr::EVRInputError CreateBooleanComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) override;
    vr::EVRInputError UpdateBooleanComponent(vr::VRInputComponentHandle_t ulComponent, bool bNewValue, double fTimeOffset) override;
    vr::EVRInputError CreateScalarComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle, vr::EVRScalarType eType, vr::EVRScalarUnits eUnits) override;
    vr::EVRInputError UpdateScalarComponent(vr::VRInputComponentHandle_t ulComponent, float fNewValue, double fTimeOffset) override;
    vr::EVRInputError CreateHapticComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) override;
    vr::EVRInputError CreateSkeletonComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, const char *pchSkeletonPath, const char *pchBasePosePath, vr::EVRSkeletalTrackingLevel eSkeletalTrackingLevel, const vr::VRBoneTransform_t *pGripLimitTransforms, uint32_t unGripLimitTransformCount, vr::VRInputComponentHandle_t *pHandle) override;
    vr::EVRInputError UpdateSkeletonComponent(vr::VRInputComponentHandle_t ulComponent, vr::EVRSkeletalMotionRange eMotionRange, const vr::VRBoneTransform_t *pTransforms, uint32_t unTransformCount) override;
    vr::EVRInputError CreatePoseComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) override;
    vr::EVRInputError UpdatePoseComponent(vr::VRInputComponentHandle_t ulComponent, const vr::HmdMatrix34_t *pMatPoseOffset, double fTimeOffset) override;
    vr::EVRInputError CreateEyeTrackingComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) override;
    vr::EVRInputError UpdateEyeTrackingComponent(vr::VRInputComponentHandle_t ulComponent, const vr::VREyeTrackingData_t *pEyeTrackingData, double fTimeOffset) override;

private:
    vr::IVRDriverInput* real_input_ = nullptr;
};

// ************************************
// RPC wrapper for vr::IVRDriverManager
// ************************************
class RpcDriverManager : public vr::IVRDriverManager, public RpcObject
{
public:
    RpcDriverManager(vr::IVRDriverManager* real);
    RpcDriverManager(RpcObjectId id);
    ~RpcDriverManager();

    RpcClassEnum GetRpcClassId() const override;

    uint32_t GetDriverCount() const override;
    uint32_t GetDriverName(vr::DriverId_t nDriver, VR_OUT_STRING() char *pchValue, uint32_t unBufferSize) override;
    vr::DriverHandle_t GetDriverHandle(const char *pchDriverName) override;
    bool IsEnabled(vr::DriverId_t nDriver) const override;

private:
    vr::IVRDriverManager* real_manager_ = nullptr;
};

// *********************************
// RPC wrapper for vr::IVRProperties
// *********************************
class RpcProperties : public vr::IVRProperties, public RpcObject
{
public:
    RpcProperties(vr::IVRProperties* real);
    RpcProperties(RpcObjectId id);
    ~RpcProperties();

    RpcClassEnum GetRpcClassId() const override;

    vr::ETrackedPropertyError ReadPropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyRead_t *pBatch, uint32_t unBatchEntryCount) override;
    vr::ETrackedPropertyError WritePropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyWrite_t *pBatch, uint32_t unBatchEntryCount) override;
    const char *GetPropErrorNameFromEnum(vr::ETrackedPropertyError error) override;
    vr::PropertyContainerHandle_t TrackedDeviceToPropertyContainer(vr::TrackedDeviceIndex_t nDevice) override;

private:
    vr::IVRProperties* real_properties_ = nullptr;
};

// ********************************
// RPC wrapper for vr::IVRResources
// ********************************
class RpcResources : public vr::IVRResources, public RpcObject
{
public:
    RpcResources(vr::IVRResources* real);
    RpcResources(RpcObjectId id);
    ~RpcResources();

    RpcClassEnum GetRpcClassId() const override;

    uint32_t LoadSharedResource(const char *pchResourceName, char *pchBuffer, uint32_t unBufferLen) override;
    uint32_t GetResourceFullPath(const char *pchResourceName, const char *pchResourceTypeDirectory, VR_OUT_STRING() char *pchPathBuffer, uint32_t unBufferLen) override;

private:
    vr::IVRResources* real_resources_ = nullptr;
};

// ****************************
// RPC wrapper for vr::IVRPaths
// ****************************
class RpcPaths : public vr::IVRPaths, public RpcObject
{
public:
    RpcPaths(vr::IVRPaths* real);
    RpcPaths(RpcObjectId id);
    ~RpcPaths();

    RpcClassEnum GetRpcClassId() const override;

    vr::ETrackedPropertyError ReadPathBatch(vr::PropertyContainerHandle_t ulRootHandle, vr::PathRead_t *pBatch, uint32_t unBatchEntryCount) override;
    vr::ETrackedPropertyError WritePathBatch(vr::PropertyContainerHandle_t ulRootHandle, vr::PathWrite_t *pBatch, uint32_t unBatchEntryCount) override;
    vr::ETrackedPropertyError StringToHandle(vr::PathHandle_t *pHandle, const char *pchPath) override;
    vr::ETrackedPropertyError HandleToString(vr::PathHandle_t pHandle, const char *pchBuffer, uint32_t unBufferSize, uint32_t *punBufferSizeUsed) override;

private:
    vr::IVRPaths* real_paths_ = nullptr;
};

// *********************************
// RPC wrapper for vr::IVRBlockQueue
// *********************************
class RpcBlockQueue : public vr::IVRBlockQueue, public RpcObject
{
public:
    RpcBlockQueue(vr::IVRBlockQueue* real);
    RpcBlockQueue(RpcObjectId id);
    ~RpcBlockQueue();

    RpcClassEnum GetRpcClassId() const override;

    vr::EBlockQueueError Create(vr::PropertyContainerHandle_t *pulQueueHandle, const char *pchPath, uint32_t unBlockDataSize, uint32_t unBlockHeaderSize, uint32_t unBlockCount, uint32_t unFlags) override;
    vr::EBlockQueueError Connect(vr::PropertyContainerHandle_t *pulQueueHandle, const char *pchPath) override;
    vr::EBlockQueueError Destroy(vr::PropertyContainerHandle_t ulQueueHandle) override;
    vr::EBlockQueueError AcquireWriteOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t *pulBlockHandle, void **ppvBuffer) override;
    vr::EBlockQueueError ReleaseWriteOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t ulBlockHandle) override;
    vr::EBlockQueueError WaitAndAcquireReadOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t *pulBlockHandle, void **ppvBuffer, vr::EBlockQueueReadType eReadType, uint32_t unTimeoutMs) override;
    vr::EBlockQueueError AcquireReadOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t *pulBlockHandle, void **ppvBuffer, vr::EBlockQueueReadType eReadType) override;
    vr::EBlockQueueError ReleaseReadOnlyBlock(vr::PropertyContainerHandle_t ulQueueHandle, vr::PropertyContainerHandle_t ulBlockHandle) override;
    vr::EBlockQueueError QueueHasReader(vr::PropertyContainerHandle_t ulQueueHandle, bool *pbHasReaders) override;

private:
    vr::IVRBlockQueue* real_block_queue_ = nullptr;
};

