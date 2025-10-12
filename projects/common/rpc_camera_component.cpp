#include "rpc_interfaces.h"

#include <iostream>
#include <vector>

// --- RpcCameraComponent ---

RpcCameraComponent::RpcCameraComponent(vr::IVRCameraComponent* real) : RpcObject(), real_component_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraFrameDimensions, [this](const auto& args) {
            uint32_t w, h;
            if (this->GetCameraFrameDimensions((vr::ECameraVideoStreamFormat)args[0].asInt(), &w, &h)) {
                uint32_t data[] = {w, h};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });
        
        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraFrameBufferingRequirements, [this](const auto& args) {
            int queue_size;
            uint32_t data_size;
            if (this->GetCameraFrameBufferingRequirements(&queue_size, &data_size)) {
                uint32_t data[] = {(uint32_t)queue_size, data_size};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_CameraComponent_StartVideoStream, [this](const auto& args) {
            return RpcValue((int)this->StartVideoStream());
        });

        this->RegisterFunction(RPCFunction_CameraComponent_StopVideoStream, [this](const auto& args) {
            this->StopVideoStream();
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_CameraComponent_IsVideoStreamActive, [this](const auto& args) {
            bool paused;
            float elapsed;
            if (this->IsVideoStreamActive(&paused, &elapsed)) {
                float data[] = {(float)paused, elapsed};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_CameraComponent_SetCameraVideoStreamFormat, [this](const auto& args) {
            return RpcValue((int)this->SetCameraVideoStreamFormat((vr::ECameraVideoStreamFormat)args[0].asInt()));
        });

        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraVideoStreamFormat, [this](const auto& args) {
            return RpcValue((int)this->GetCameraVideoStreamFormat());
        });

        this->RegisterFunction(RPCFunction_CameraComponent_SetAutoExposure, [this](const auto& args) {
            return RpcValue((int)this->SetAutoExposure(args[0].asInt()));
        });

        this->RegisterFunction(RPCFunction_CameraComponent_PauseVideoStream, [this](const auto& args) {
            return RpcValue((int)this->PauseVideoStream());
        });

        this->RegisterFunction(RPCFunction_CameraComponent_ResumeVideoStream, [this](const auto& args) {
            return RpcValue((int)this->ResumeVideoStream());
        });

        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraDistortion, [this](const auto& args) {
            float out_u, out_v;
            if (this->GetCameraDistortion((uint32_t)args[0].asInt(), args[1].asFloat(), args[2].asFloat(), &out_u, &out_v)) {
                float data[] = {out_u, out_v};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraProjection, [this](const auto& args) {
            vr::HmdMatrix44_t proj;
            if (this->GetCameraProjection((uint32_t)args[0].asInt(), (vr::EVRTrackedCameraFrameType)args[1].asInt(), args[2].asFloat(), args[3].asFloat(), &proj)) {
                return RpcValue((const char*)&proj, sizeof(proj));
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_CameraComponent_SetFrameRate, [this](const auto& args) {
            return RpcValue((int)this->SetFrameRate(args[0].asInt(), args[1].asInt()));
        });

        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraCompatibilityMode, [this](const auto& args) {
            vr::ECameraCompatibilityMode mode;
            if (this->GetCameraCompatibilityMode(&mode)) {
                return RpcValue((int)mode);
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_CameraComponent_SetCameraCompatibilityMode, [this](const auto& args) {
            return RpcValue((int)this->SetCameraCompatibilityMode((vr::ECameraCompatibilityMode)args[0].asInt()));
        });

        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraFrameBounds, [this](const auto& args) {
            uint32_t l, t, w, h;
            if (this->GetCameraFrameBounds((vr::EVRTrackedCameraFrameType)args[0].asInt(), &l, &t, &w, &h)) {
                uint32_t data[] = {l, t, w, h};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        this->RegisterFunction(RPCFunction_CameraComponent_GetCameraIntrinsics, [this](const auto& args) {
            vr::HmdVector2_t focal_length, center;
            vr::EVRDistortionFunctionType dist_type;
            double coeffs[vr::k_unMaxDistortionFunctionParameters];
            if (this->GetCameraIntrinsics((uint32_t)args[0].asInt(), (vr::EVRTrackedCameraFrameType)args[1].asInt(), &focal_length, &center, &dist_type, coeffs)) {
                std::vector<char> buffer;
                buffer.insert(buffer.end(), (char*)&focal_length, (char*)&focal_length + sizeof(focal_length));
                buffer.insert(buffer.end(), (char*)&center, (char*)&center + sizeof(center));
                buffer.insert(buffer.end(), (char*)&dist_type, (char*)&dist_type + sizeof(dist_type));
                buffer.insert(buffer.end(), (char*)coeffs, (char*)coeffs + sizeof(coeffs));
                return RpcValue(buffer.data(), buffer.size());
            }
            return RpcValue();
        });

        // Stubs for currently unsupported methods
        this->RegisterFunction(RPCFunction_CameraComponent_SetCameraFrameBuffering, [](const auto& args){ return RpcValue(0); });
        this->RegisterFunction(RPCFunction_CameraComponent_GetVideoStreamFrame, [](const auto& args){ return RpcValue(); });
        this->RegisterFunction(RPCFunction_CameraComponent_ReleaseVideoStreamFrame, [](const auto& args){ return RpcValue(); });
        this->RegisterFunction(RPCFunction_CameraComponent_SetCameraVideoSinkCallback, [](const auto& args){ return RpcValue(0); });
    }
}
RpcCameraComponent::RpcCameraComponent(RpcObjectId id) : RpcObject(id) {}
RpcCameraComponent::~RpcCameraComponent() {}

RpcClassEnum RpcCameraComponent::GetRpcClassId() const {
    return RPCClassCameraComponent;
}

bool RpcCameraComponent::GetCameraFrameDimensions(vr::ECameraVideoStreamFormat nVideoStreamFormat, uint32_t *pWidth, uint32_t *pHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraFrameDimensions, RpcValue((int)nVideoStreamFormat));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asByteArray().data());
            *pWidth = data[0]; *pHeight = data[1];
            return true;
        }
        return false;
    }
    else {
        return real_component_->GetCameraFrameDimensions(nVideoStreamFormat, pWidth, pHeight);
    }
}

bool RpcCameraComponent::GetCameraFrameBufferingRequirements(int *pDefaultFrameQueueSize, uint32_t *pFrameBufferDataSize) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraFrameBufferingRequirements);
        if (result.isByteArray() && result.asByteArray().size() == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asByteArray().data());
            *pDefaultFrameQueueSize = data[0]; *pFrameBufferDataSize = data[1];
            return true;
        }
        return false;
    }
    else {
        return real_component_->GetCameraFrameBufferingRequirements(pDefaultFrameQueueSize, pFrameBufferDataSize);
    }
}

bool RpcCameraComponent::StartVideoStream() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_StartVideoStream).asInt() :
        real_component_->StartVideoStream();
}

void RpcCameraComponent::StopVideoStream() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_StopVideoStream);
    }
    else {
        real_component_->StopVideoStream();
    }
}

bool RpcCameraComponent::IsVideoStreamActive(bool *pbPaused, float *pflElapsedTime) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_IsVideoStreamActive);
        if (result.isByteArray() && result.asByteArray().size() == sizeof(float) * 2) {
            const float* data = reinterpret_cast<const float*>(result.asByteArray().data());
            *pbPaused = (bool)data[0]; *pflElapsedTime = data[1];
            return true;
        }
        return false;
    }
    else {
        return real_component_->IsVideoStreamActive(pbPaused, pflElapsedTime);
    }
}

bool RpcCameraComponent::SetCameraFrameBuffering(int nFrameBufferCount, void **ppFrameBuffers, uint32_t nFrameBufferDataSize) {
    // Stub: This is too complex to marshal over RPC. Frame buffers are memory regions that would need to be shared.
    // Also may not be used at all?

    std::cout << "Warning: SetCameraFrameBuffering called on RPC proxy, but this is a stub." << std::endl;
    return false;
}

bool RpcCameraComponent::SetCameraVideoStreamFormat(vr::ECameraVideoStreamFormat nVideoStreamFormat) {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_SetCameraVideoStreamFormat, RpcValue((int)nVideoStreamFormat)).asInt() : real_component_->SetCameraVideoStreamFormat(nVideoStreamFormat);
}

vr::ECameraVideoStreamFormat RpcCameraComponent::GetCameraVideoStreamFormat() {
    return IsProxy() ? (vr::ECameraVideoStreamFormat)RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraVideoStreamFormat).asInt() : real_component_->GetCameraVideoStreamFormat();
}

const vr::CameraVideoStreamFrame_t *RpcCameraComponent::GetVideoStreamFrame() {
    // Stub: Returning a pointer to a complex struct that contains image data is not feasible with this RPC system.
    // A more advanced implementation would involve serializing the frame data and managing its lifecycle across processes.
    // Also may not be used at all?

    std::cout << "Warning: GetVideoStreamFrame called on RPC proxy, but this is a stub." << std::endl;
    return nullptr;
}

void RpcCameraComponent::ReleaseVideoStreamFrame(const vr::CameraVideoStreamFrame_t *pFrameImage) {
    // Stub: Companion to GetVideoStreamFrame.
    // Also may not be used at all?

    std::cout << "Warning: ReleaseVideoStreamFrame called on RPC proxy, but this is a stub." << std::endl;
}

bool RpcCameraComponent::SetAutoExposure(bool bEnable) {
    return IsProxy() ? 
        RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_SetAutoExposure, RpcValue((int)bEnable)).asInt() :
        real_component_->SetAutoExposure(bEnable);
}

bool RpcCameraComponent::PauseVideoStream() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_PauseVideoStream).asInt() :
        real_component_->PauseVideoStream();
}

bool RpcCameraComponent::ResumeVideoStream() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_ResumeVideoStream).asInt() :
        real_component_->ResumeVideoStream();
}

bool RpcCameraComponent::GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float *pflOutputU, float *pflOutputV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraDistortion, RpcValue((int)nCameraIndex), RpcValue(flInputU), RpcValue(flInputV));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(float) * 2) {
            const float* data = reinterpret_cast<const float*>(result.asByteArray().data());
            *pflOutputU = data[0]; *pflOutputV = data[1];
            return true;
        }
        return false;
    }
    else {
        return real_component_->GetCameraDistortion(nCameraIndex, flInputU, flInputV, pflOutputU, pflOutputV);
    }
}

bool RpcCameraComponent::GetCameraProjection(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, float flZNear, float flZFar, vr::HmdMatrix44_t *pProjection) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraProjection, RpcValue((int)nCameraIndex), RpcValue((int)eFrameType), RpcValue(flZNear), RpcValue(flZFar));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(vr::HmdMatrix44_t)) {
            *pProjection = *reinterpret_cast<const vr::HmdMatrix44_t*>(result.asByteArray().data());
            return true;
        }
        return false;
    }
    else {
        return real_component_->GetCameraProjection(nCameraIndex, eFrameType, flZNear, flZFar, pProjection);
    }
}

bool RpcCameraComponent::SetFrameRate(int nISPFrameRate, int nSensorFrameRate) {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_SetFrameRate, RpcValue(nISPFrameRate), RpcValue(nSensorFrameRate)).asInt() :
        real_component_->SetFrameRate(nISPFrameRate, nSensorFrameRate);
}

bool RpcCameraComponent::SetCameraVideoSinkCallback(vr::ICameraVideoSinkCallback *pCameraVideoSinkCallback) {
    // Stub: This may not be used at all.
    return false;
}

bool RpcCameraComponent::GetCameraCompatibilityMode(vr::ECameraCompatibilityMode *pCameraCompatibilityMode) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraCompatibilityMode);
        if (result.isInt()) {
            *pCameraCompatibilityMode = (vr::ECameraCompatibilityMode)result.asInt();
            return true;
        }
        return false;
    }
    else {
        return real_component_->GetCameraCompatibilityMode(pCameraCompatibilityMode);
    }
}

bool RpcCameraComponent::SetCameraCompatibilityMode(vr::ECameraCompatibilityMode nCameraCompatibilityMode) {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_SetCameraCompatibilityMode, RpcValue((int)nCameraCompatibilityMode)).asInt() :
        real_component_->SetCameraCompatibilityMode(nCameraCompatibilityMode);
}

bool RpcCameraComponent::GetCameraFrameBounds(vr::EVRTrackedCameraFrameType eFrameType, uint32_t *pLeft, uint32_t *pTop, uint32_t *pWidth, uint32_t *pHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraFrameBounds, RpcValue((int)eFrameType));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(uint32_t) * 4) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asByteArray().data());
            *pLeft = data[0]; *pTop = data[1]; *pWidth = data[2]; *pHeight = data[3];
            return true;
        }
        return false;
    }
    else {
        return real_component_->GetCameraFrameBounds(eFrameType, pLeft, pTop, pWidth, pHeight);
    }
}

bool RpcCameraComponent::GetCameraIntrinsics(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, vr::HmdVector2_t *pFocalLength, vr::HmdVector2_t *pCenter, vr::EVRDistortionFunctionType *peDistortionType, double rCoefficients[vr::k_unMaxDistortionFunctionParameters]) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_CameraComponent_GetCameraIntrinsics, RpcValue((int)nCameraIndex), RpcValue((int)eFrameType));
        if (result.isByteArray()) {
            auto data = result.asByteArray();
            const char* ptr = data.data();
            size_t expected_size = sizeof(vr::HmdVector2_t) * 2 + sizeof(vr::EVRDistortionFunctionType) + sizeof(double) * vr::k_unMaxDistortionFunctionParameters;
            if (data.size() == expected_size) {
                memcpy(pFocalLength, ptr, sizeof(vr::HmdVector2_t));
                ptr += sizeof(vr::HmdVector2_t);
                memcpy(pCenter, ptr, sizeof(vr::HmdVector2_t));
                ptr += sizeof(vr::HmdVector2_t);
                memcpy(peDistortionType, ptr, sizeof(vr::EVRDistortionFunctionType));
                ptr += sizeof(vr::EVRDistortionFunctionType);
                memcpy(rCoefficients, ptr, sizeof(double) * vr::k_unMaxDistortionFunctionParameters);
                return true;
            }
        }
        return false;
    }
    else {
        return real_component_->GetCameraIntrinsics(nCameraIndex, eFrameType, pFocalLength, pCenter, peDistortionType, rCoefficients);
    }
}
