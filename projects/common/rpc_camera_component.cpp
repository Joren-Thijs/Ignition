#include "vr_rpc_interfaces.h"
#include <vector>

// --- RpcCameraComponent ---

RpcCameraComponent::RpcCameraComponent(vr::IVRCameraComponent* real) : RpcObject(), real_component_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";

        RpcSystem::RegisterFunction(prefix + "GetCameraFrameDimensions", [this](const auto& args) {
            uint32_t w, h;
            if (this->GetCameraFrameDimensions((vr::ECameraVideoStreamFormat)args[0].asInt(), &w, &h)) {
                uint32_t data[] = {w, h};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });
        
        RpcSystem::RegisterFunction(prefix + "GetCameraFrameBufferingRequirements", [this](const auto& args) {
            int queue_size;
            uint32_t data_size;
            if (this->GetCameraFrameBufferingRequirements(&queue_size, &data_size)) {
                uint32_t data[] = {(uint32_t)queue_size, data_size};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "StartVideoStream", [this](const auto& args) {
            return RpcValue((int)this->StartVideoStream());
        });

        RpcSystem::RegisterFunction(prefix + "StopVideoStream", [this](const auto& args) {
            this->StopVideoStream();
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "IsVideoStreamActive", [this](const auto& args) {
            bool paused;
            float elapsed;
            if (this->IsVideoStreamActive(&paused, &elapsed)) {
                float data[] = {(float)paused, elapsed};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "SetCameraVideoStreamFormat", [this](const auto& args) {
            return RpcValue((int)this->SetCameraVideoStreamFormat((vr::ECameraVideoStreamFormat)args[0].asInt()));
        });

        RpcSystem::RegisterFunction(prefix + "GetCameraVideoStreamFormat", [this](const auto& args) {
            return RpcValue((int)this->GetCameraVideoStreamFormat());
        });

        RpcSystem::RegisterFunction(prefix + "SetAutoExposure", [this](const auto& args) {
            return RpcValue((int)this->SetAutoExposure(args[0].asInt()));
        });

        RpcSystem::RegisterFunction(prefix + "PauseVideoStream", [this](const auto& args) {
            return RpcValue((int)this->PauseVideoStream());
        });

        RpcSystem::RegisterFunction(prefix + "ResumeVideoStream", [this](const auto& args) {
            return RpcValue((int)this->ResumeVideoStream());
        });

        RpcSystem::RegisterFunction(prefix + "GetCameraDistortion", [this](const auto& args) {
            float out_u, out_v;
            if (this->GetCameraDistortion((uint32_t)args[0].asInt(), args[1].asFloat(), args[2].asFloat(), &out_u, &out_v)) {
                float data[] = {out_u, out_v};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "GetCameraProjection", [this](const auto& args) {
            vr::HmdMatrix44_t proj;
            if (this->GetCameraProjection((uint32_t)args[0].asInt(), (vr::EVRTrackedCameraFrameType)args[1].asInt(), args[2].asFloat(), args[3].asFloat(), &proj)) {
                return RpcValue((const char*)&proj, sizeof(proj));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "SetFrameRate", [this](const auto& args) {
            return RpcValue((int)this->SetFrameRate(args[0].asInt(), args[1].asInt()));
        });

        RpcSystem::RegisterFunction(prefix + "GetCameraCompatibilityMode", [this](const auto& args) {
            vr::ECameraCompatibilityMode mode;
            if (this->GetCameraCompatibilityMode(&mode)) {
                return RpcValue((int)mode);
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "SetCameraCompatibilityMode", [this](const auto& args) {
            return RpcValue((int)this->SetCameraCompatibilityMode((vr::ECameraCompatibilityMode)args[0].asInt()));
        });

        RpcSystem::RegisterFunction(prefix + "GetCameraFrameBounds", [this](const auto& args) {
            uint32_t l, t, w, h;
            if (this->GetCameraFrameBounds((vr::EVRTrackedCameraFrameType)args[0].asInt(), &l, &t, &w, &h)) {
                uint32_t data[] = {l, t, w, h};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "GetCameraIntrinsics", [this](const auto& args) {
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
        RpcSystem::RegisterFunction(prefix + "SetCameraFrameBuffering", [](const auto& args){ return RpcValue(0); });
        RpcSystem::RegisterFunction(prefix + "GetVideoStreamFrame", [](const auto& args){ return RpcValue(); });
        RpcSystem::RegisterFunction(prefix + "ReleaseVideoStreamFrame", [](const auto& args){ return RpcValue(); });
        RpcSystem::RegisterFunction(prefix + "SetCameraVideoSinkCallback", [](const auto& args){ return RpcValue(0); });
    }
}
RpcCameraComponent::RpcCameraComponent(RpcObjectId id) : RpcObject(id) {}
RpcCameraComponent::~RpcCameraComponent() {}

const std::string& RpcCameraComponent::GetRpcClassName() const {
    static const std::string name = "IVRCameraComponent";
    return name;
}

bool RpcCameraComponent::GetCameraFrameDimensions(vr::ECameraVideoStreamFormat nVideoStreamFormat, uint32_t *pWidth, uint32_t *pHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraFrameDimensions", RpcValue((int)nVideoStreamFormat));
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
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
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraFrameBufferingRequirements");
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
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
        RpcSystem::CallMethod(GetId(), "StartVideoStream").asInt() :
        real_component_->StartVideoStream();
}

void RpcCameraComponent::StopVideoStream() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "StopVideoStream");
    }
    else {
        real_component_->StopVideoStream();
    }
}

bool RpcCameraComponent::IsVideoStreamActive(bool *pbPaused, float *pflElapsedTime) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "IsVideoStreamActive");
        if (result.isPointer() && result.asPointer().second == sizeof(float) * 2) {
            const float* data = reinterpret_cast<const float*>(result.asPointer().first);
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
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "SetCameraVideoStreamFormat", RpcValue((int)nVideoStreamFormat)).asInt() : real_component_->SetCameraVideoStreamFormat(nVideoStreamFormat);
}

vr::ECameraVideoStreamFormat RpcCameraComponent::GetCameraVideoStreamFormat() {
    return IsProxy() ? (vr::ECameraVideoStreamFormat)RpcSystem::CallMethod(GetId(), "GetCameraVideoStreamFormat").asInt() : real_component_->GetCameraVideoStreamFormat();
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
        RpcSystem::CallMethod(GetId(), "SetAutoExposure", RpcValue((int)bEnable)).asInt() :
        real_component_->SetAutoExposure(bEnable);
}

bool RpcCameraComponent::PauseVideoStream() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), "PauseVideoStream").asInt() :
        real_component_->PauseVideoStream();
}

bool RpcCameraComponent::ResumeVideoStream() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), "ResumeVideoStream").asInt() :
        real_component_->ResumeVideoStream();
}

bool RpcCameraComponent::GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float *pflOutputU, float *pflOutputV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraDistortion", RpcValue((int)nCameraIndex), RpcValue(flInputU), RpcValue(flInputV));
        if (result.isPointer() && result.asPointer().second == sizeof(float) * 2) {
            const float* data = reinterpret_cast<const float*>(result.asPointer().first);
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
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraProjection", RpcValue((int)nCameraIndex), RpcValue((int)eFrameType), RpcValue(flZNear), RpcValue(flZFar));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::HmdMatrix44_t)) {
            *pProjection = *reinterpret_cast<const vr::HmdMatrix44_t*>(result.asPointer().first);
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
        RpcSystem::CallMethod(GetId(), "SetFrameRate", RpcValue(nISPFrameRate), RpcValue(nSensorFrameRate)).asInt() :
        real_component_->SetFrameRate(nISPFrameRate, nSensorFrameRate);
}

bool RpcCameraComponent::SetCameraVideoSinkCallback(vr::ICameraVideoSinkCallback *pCameraVideoSinkCallback) {
    // Stub: This may not be used at all.
    return false;
}

bool RpcCameraComponent::GetCameraCompatibilityMode(vr::ECameraCompatibilityMode *pCameraCompatibilityMode) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraCompatibilityMode");
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
        RpcSystem::CallMethod(GetId(), "SetCameraCompatibilityMode", RpcValue((int)nCameraCompatibilityMode)).asInt() :
        real_component_->SetCameraCompatibilityMode(nCameraCompatibilityMode);
}

bool RpcCameraComponent::GetCameraFrameBounds(vr::EVRTrackedCameraFrameType eFrameType, uint32_t *pLeft, uint32_t *pTop, uint32_t *pWidth, uint32_t *pHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraFrameBounds", RpcValue((int)eFrameType));
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 4) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
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
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraIntrinsics", RpcValue((int)nCameraIndex), RpcValue((int)eFrameType));
        if (result.isPointer()) {
            auto data = result.asPointer();
            const char* ptr = data.first;
            size_t expected_size = sizeof(vr::HmdVector2_t) * 2 + sizeof(vr::EVRDistortionFunctionType) + sizeof(double) * vr::k_unMaxDistortionFunctionParameters;
            if (data.second == expected_size) {
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
