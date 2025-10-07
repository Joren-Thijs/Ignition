#include "vr_rpc_interfaces.h"
#include <iostream>
#include <vector>

// Needed to fix issue with how Valve didn't pack VREvent_t correctly on Linux. Thanks, Valve!
struct VREvent_t8
{
	uint32_t eventType; // EVREventType enum
	vr::TrackedDeviceIndex_t trackedDeviceIndex;
	float eventAgeSeconds;
	// event data must be the end of the struct as its size is variable
	vr::VREvent_Data_t data;
};

// --- RpcDriverHost ---

RpcDriverHost::RpcDriverHost(vr::IVRServerDriverHost* real) : RpcObject(), real_host_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_DriverHost_TrackedDeviceAdded, [this](const auto& args) {
            const std::string serial_str = args[0].asString();
            const char* serial = serial_str.c_str();
            auto device_class = (vr::ETrackedDeviceClass)args[1].asInt();
            auto* driver_proxy = static_cast<RpcTrackedDeviceServerDriver*>(args[2].asObject());

            bool result = this->TrackedDeviceAdded(serial, device_class, driver_proxy);
            return RpcValue((int)result);
        });

        this->RegisterFunction(RPCFunction_DriverHost_TrackedDevicePoseUpdated, [this](const auto& args) {
            uint32_t which_device = (uint32_t)args[0].asInt();
            auto pose_data = args[1].asPointer();
            vr::DriverPose_t new_pose;
            if (pose_data.second == sizeof(vr::DriverPose_t)) {
                memcpy(&new_pose, pose_data.first, sizeof(vr::DriverPose_t));
                uint32_t pose_size = sizeof(vr::DriverPose_t);
                this->TrackedDevicePoseUpdated(which_device, new_pose, pose_size);
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_VendorSpecificEvent, [this](const auto& args) {
            uint32_t unWhichDevice = (uint32_t)args[0].asInt();
            auto eventType = (vr::EVREventType)args[1].asInt();
            auto eventDataPair = args[2].asPointer();
            double eventTimeOffset = (double)args[3].asDouble();

            vr::VREvent_Data_t eventData;
            if (eventDataPair.second == sizeof(vr::VREvent_Data_t))
            {
                memcpy(&eventData, eventDataPair.first, sizeof(vr::VREvent_Data_t));
                this->VendorSpecificEvent(unWhichDevice, eventType, eventData, eventTimeOffset);
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_VsyncEvent, [this](const auto& args) {
            this->VsyncEvent(args[0].asDouble());
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_IsExiting, [this](const auto& args) {
            return RpcValue((int)this->IsExiting());
        });

        this->RegisterFunction(RPCFunction_DriverHost_RequestRestart, [this](const auto& args) {
            this->RequestRestart(args[0].asString().c_str(), args[1].asString().c_str(), args[2].asString().c_str(), args[3].asString().c_str());
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_SetRecommendedRenderTargetSize, [this](const auto& args) {
            this->SetRecommendedRenderTargetSize((uint32_t)args[0].asInt(), (uint32_t)args[1].asInt(), (uint32_t)args[2].asInt());
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_PollNextEvent, [this](const auto& args) {
            vr::VREvent_t event;
            VREvent_t8 event8;

            int expected_size = args[0].asInt();

            if (this->PollNextEvent(&event, sizeof(event))) {
                // Copy members over
                event8.eventType = event.eventType;
                event8.trackedDeviceIndex = event.trackedDeviceIndex;
                event8.eventAgeSeconds = event.eventAgeSeconds;
                event8.data = event.data;

                return RpcValue((const char*)&event8, sizeof(event8));
            }

            return RpcValue(); // Return null on no event
        });

        this->RegisterFunction(RPCFunction_DriverHost_GetRawTrackedDevicePoses, [this](const auto& args) {
            float fPredictedSecondsFromNow = args[0].asFloat();
            uint32_t unTrackedDevicePoseArrayCount = (uint32_t)args[1].asInt();

            if (unTrackedDevicePoseArrayCount > 0) {
                std::vector<vr::TrackedDevicePose_t> poses(unTrackedDevicePoseArrayCount);
                this->GetRawTrackedDevicePoses(fPredictedSecondsFromNow, poses.data(), unTrackedDevicePoseArrayCount);
                return RpcValue((const char*)poses.data(), poses.size() * sizeof(vr::TrackedDevicePose_t));
            }

            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_GetFrameTimings, [this](const auto& args) {
            uint32_t nFrames = (uint32_t)args[0].asInt();

            if (nFrames > 0) {
                std::vector<vr::Compositor_FrameTiming> timings(nFrames);
                uint32_t framesRead = this->GetFrameTimings(timings.data(), nFrames);
                // Return a pair: number of frames read, and the data buffer
                return RpcValue((const char*)timings.data(), framesRead * sizeof(vr::Compositor_FrameTiming));
            }

            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_SetDisplayEyeToHead, [this](const auto& args) {
            uint32_t unWhichDevice = (uint32_t)args[0].asInt();
            auto leftEyeData = args[1].asPointer();
            auto rightEyeData = args[2].asPointer();

            if (leftEyeData.second == sizeof(vr::HmdMatrix34_t) && rightEyeData.second == sizeof(vr::HmdMatrix34_t)) {
                this->SetDisplayEyeToHead(unWhichDevice, *reinterpret_cast<const vr::HmdMatrix34_t*>(leftEyeData.first), *reinterpret_cast<const vr::HmdMatrix34_t*>(rightEyeData.first));
            }

            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_DriverHost_SetDisplayProjectionRaw, [this](const auto& args) {
            uint32_t unWhichDevice = (uint32_t)args[0].asInt();
            auto leftEyeData = args[1].asPointer();
            auto rightEyeData = args[2].asPointer();

            if (leftEyeData.second == sizeof(vr::HmdRect2_t) && rightEyeData.second == sizeof(vr::HmdRect2_t)) {
                this->SetDisplayProjectionRaw(unWhichDevice, *reinterpret_cast<const vr::HmdRect2_t*>(leftEyeData.first), *reinterpret_cast<const vr::HmdRect2_t*>(rightEyeData.first));
            }

            return RpcValue();
        });
    }
}

RpcDriverHost::RpcDriverHost(RpcObjectId id) : RpcObject(id) {}

RpcDriverHost::~RpcDriverHost() {}

RpcClassEnum RpcDriverHost::GetRpcClassId() const {
    return Class_DriverHost;
}

bool RpcDriverHost::TrackedDeviceAdded(const char *pchDeviceSerialNumber, vr::ETrackedDeviceClass eDeviceClass, vr::ITrackedDeviceServerDriver *pDriver) {
    if (IsProxy()) {
        // This is called on the server (proxy) by the real driver.
        // We need to forward this to the client.
        // We wrap the server-side pDriver in an Rpc-enabled object.
        auto* rpc_driver = new RpcTrackedDeviceServerDriver(pDriver);

        // Now we can call the client's real TrackedDeviceAdded with our proxy object.
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_TrackedDeviceAdded, RpcValue(std::string(pchDeviceSerialNumber)), RpcValue((int)eDeviceClass), RpcValue(rpc_driver));
        return result.asInt();
    }
    else {
        return real_host_->TrackedDeviceAdded(pchDeviceSerialNumber, eDeviceClass, pDriver);
    }
}

void RpcDriverHost::TrackedDevicePoseUpdated(uint32_t unWhichDevice, const vr::DriverPose_t &newPose, uint32_t unPoseStructSize) {
    if (IsProxy()) {
        // This is called on the server (proxy) by the real driver.
        // We need to forward this to the client.
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_TrackedDevicePoseUpdated, RpcValue((int)unWhichDevice), RpcValue((const char*)&newPose, unPoseStructSize));
    }
    else {
        real_host_->TrackedDevicePoseUpdated(unWhichDevice, newPose, unPoseStructSize);
    }
}

void RpcDriverHost::VsyncEvent(double vsyncTimeOffsetSeconds) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_VsyncEvent, RpcValue(vsyncTimeOffsetSeconds));
    }
    else {
        real_host_->VsyncEvent(vsyncTimeOffsetSeconds);
    }
}

void RpcDriverHost::VendorSpecificEvent(uint32_t unWhichDevice, vr::EVREventType eventType, const vr::VREvent_Data_t &eventData, double eventTimeOffset) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_VendorSpecificEvent,
            RpcValue((int)unWhichDevice),
            RpcValue((int)eventType),
            RpcValue((const char*)&eventData, sizeof(eventData)),
            RpcValue(eventTimeOffset)
        );
    }
    else {
        real_host_->VendorSpecificEvent(unWhichDevice, eventType, eventData, eventTimeOffset);
    }
}

bool RpcDriverHost::IsExiting() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_IsExiting).asInt() :
        real_host_->IsExiting();
}

bool RpcDriverHost::PollNextEvent(vr::VREvent_t *pEvent, uint32_t uncbVREvent) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_PollNextEvent, RpcValue((int)uncbVREvent));
        if (result.isPointer()) {
            auto data = result.asPointer();
            if (data.second == sizeof(vr::VREvent_t)) {
                memcpy(pEvent, data.first, data.second);
                return true;
            }
            else {
                std::cerr << "Warning: PollNextEvent returned data of unexpected size " << data.second << std::endl;
            }
        }
        return false; // No event
    }
    else {
        return real_host_->PollNextEvent(pEvent, uncbVREvent);
    }
}

void RpcDriverHost::GetRawTrackedDevicePoses(float fPredictedSecondsFromNow, vr::TrackedDevicePose_t *pTrackedDevicePoseArray, uint32_t unTrackedDevicePoseArrayCount) {
    if (IsProxy()) {
        if (!pTrackedDevicePoseArray || unTrackedDevicePoseArrayCount == 0) {
            return;
        }
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_GetRawTrackedDevicePoses, RpcValue(fPredictedSecondsFromNow), RpcValue((int)unTrackedDevicePoseArrayCount));
        if (result.isPointer()) {
            auto data = result.asPointer();
            size_t bytesToCopy = std::min(data.second, (size_t)unTrackedDevicePoseArrayCount * sizeof(vr::TrackedDevicePose_t));
            memcpy(pTrackedDevicePoseArray, data.first, bytesToCopy);
        }
    }
    else {
        real_host_->GetRawTrackedDevicePoses(fPredictedSecondsFromNow, pTrackedDevicePoseArray, unTrackedDevicePoseArrayCount);
    }
}

void RpcDriverHost::RequestRestart(const char *pchLocalizedReason, const char *pchExecutableToStart, const char *pchArguments, const char *pchWorkingDirectory) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_RequestRestart, RpcValue(std::string(pchLocalizedReason ? pchLocalizedReason : "")), RpcValue(std::string(pchExecutableToStart ? pchExecutableToStart : "")), RpcValue(std::string(pchArguments ? pchArguments : "")), RpcValue(std::string(pchWorkingDirectory ? pchWorkingDirectory : "")));
    }
    else {
        real_host_->RequestRestart(pchLocalizedReason, pchExecutableToStart, pchArguments, pchWorkingDirectory);
    }
}

uint32_t RpcDriverHost::GetFrameTimings(vr::Compositor_FrameTiming *pTiming, uint32_t nFrames) {
    if (IsProxy()) {
        if (!pTiming || nFrames == 0) {
            return 0;
        }
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_GetFrameTimings, RpcValue((int)nFrames));
        if (result.isPointer()) {
            auto data = result.asPointer();
            uint32_t framesRead = data.second / sizeof(vr::Compositor_FrameTiming);
            uint32_t framesToCopy = std::min(nFrames, framesRead);
            memcpy(pTiming, data.first, framesToCopy * sizeof(vr::Compositor_FrameTiming));
            return framesToCopy;
        }
        return 0;
    }
    else {
        return real_host_->GetFrameTimings(pTiming, nFrames);
    }
}

void RpcDriverHost::SetDisplayEyeToHead(uint32_t unWhichDevice, const vr::HmdMatrix34_t &eyeToHeadLeft, const vr::HmdMatrix34_t &eyeToHeadRight) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_SetDisplayEyeToHead,
            RpcValue((int)unWhichDevice),
            RpcValue((const char*)&eyeToHeadLeft, sizeof(eyeToHeadLeft)),
            RpcValue((const char*)&eyeToHeadRight, sizeof(eyeToHeadRight))
        );
    }
    else {
        real_host_->SetDisplayEyeToHead(unWhichDevice, eyeToHeadLeft, eyeToHeadRight);
    }
}

void RpcDriverHost::SetDisplayProjectionRaw(uint32_t unWhichDevice, const vr::HmdRect2_t &eyeLeft, const vr::HmdRect2_t &eyeRight) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_SetDisplayProjectionRaw,
            RpcValue((int)unWhichDevice),
            RpcValue((const char*)&eyeLeft, sizeof(eyeLeft)),
            RpcValue((const char*)&eyeRight, sizeof(eyeRight))
        );
    }
    else {
        real_host_->SetDisplayProjectionRaw(unWhichDevice, eyeLeft, eyeRight);
    }
}

void RpcDriverHost::SetRecommendedRenderTargetSize(uint32_t unWhichDevice, uint32_t nWidth, uint32_t nHeight) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverHost_SetRecommendedRenderTargetSize, RpcValue((int)unWhichDevice), RpcValue((int)nWidth), RpcValue((int)nHeight));
    }
    else {
        real_host_->SetRecommendedRenderTargetSize(unWhichDevice, nWidth, nHeight);
    }
}
