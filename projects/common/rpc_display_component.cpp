#include "vr_rpc_interfaces.h"

// --- RpcDisplayComponent ---

RpcDisplayComponent::RpcDisplayComponent(vr::IVRDisplayComponent* real) : RpcObject(), real_component_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";

        RpcSystem::RegisterFunction(prefix + "GetWindowBounds", [this](const auto& args) {
            int32_t x, y;
            uint32_t w, h;
            this->GetWindowBounds(&x, &y, &w, &h);
            int32_t data[] = {x, y, (int32_t)w, (int32_t)h};
            return RpcValue((const char*)data, sizeof(data));
        });

        RpcSystem::RegisterFunction(prefix + "IsDisplayOnDesktop", [this](const auto& args) {
            return RpcValue((int)this->IsDisplayOnDesktop());
        });
        
        RpcSystem::RegisterFunction(prefix + "IsDisplayRealDisplay", [this](const auto& args) {
            return RpcValue((int)this->IsDisplayRealDisplay());
        });

        RpcSystem::RegisterFunction(prefix + "GetRecommendedRenderTargetSize", [this](const auto& args) {
            uint32_t w, h;
            this->GetRecommendedRenderTargetSize(&w, &h);
            uint32_t data[] = {w, h};
            return RpcValue((const char*)data, sizeof(data));
        });

        RpcSystem::RegisterFunction(prefix + "GetEyeOutputViewport", [this](const auto& args) {
            uint32_t x, y, w, h;
            this->GetEyeOutputViewport((vr::EVREye)args[0].asInt(), &x, &y, &w, &h);
            uint32_t data[] = {x, y, w, h};
            return RpcValue((const char*)data, sizeof(data));
        });

        RpcSystem::RegisterFunction(prefix + "GetProjectionRaw", [this](const auto& args) {
            float l, r, t, b;
            this->GetProjectionRaw((vr::EVREye)args[0].asInt(), &l, &r, &t, &b);
            float data[] = {l, r, t, b};
            return RpcValue((const char*)data, sizeof(data));
        });

        RpcSystem::RegisterFunction(prefix + "ComputeDistortion", [this](const auto& args) {
            vr::DistortionCoordinates_t coords = this->ComputeDistortion((vr::EVREye)args[0].asInt(), args[1].asFloat(), args[2].asFloat());
            return RpcValue((const char*)&coords, sizeof(coords));
        });
        
        RpcSystem::RegisterFunction(prefix + "ComputeInverseDistortion", [this](const auto& args) {
            vr::HmdVector2_t result;
            if (this->ComputeInverseDistortion(&result, (vr::EVREye)args[0].asInt(), (uint32_t)args[1].asInt(), args[2].asFloat(), args[3].asFloat())) {
                return RpcValue((const char*)&result, sizeof(result));
            }
            return RpcValue(); // Return null on failure
        });
    }
}
RpcDisplayComponent::RpcDisplayComponent(RpcObjectId id) : RpcObject(id) {}
RpcDisplayComponent::~RpcDisplayComponent() {}

const std::string& RpcDisplayComponent::GetRpcClassName() const {
    static const std::string name = "IVRDisplayComponent";
    return name;
}

void RpcDisplayComponent::GetWindowBounds(int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetWindowBounds");
        if (result.isPointer() && result.asPointer().second == sizeof(int32_t) * 4) {
            const int32_t* data = reinterpret_cast<const int32_t*>(result.asPointer().first);
            *pnX = data[0]; *pnY = data[1]; *pnWidth = data[2]; *pnHeight = data[3];
        }
    }
    else {
        real_component_->GetWindowBounds(pnX, pnY, pnWidth, pnHeight);
    }
}

bool RpcDisplayComponent::IsDisplayOnDesktop() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), "IsDisplayOnDesktop").asInt() :
        real_component_->IsDisplayOnDesktop();
}

bool RpcDisplayComponent::IsDisplayRealDisplay() {
    return IsProxy() ? 
        RpcSystem::CallMethod(GetId(), "IsDisplayRealDisplay").asInt() :
        real_component_->IsDisplayRealDisplay();
}

void RpcDisplayComponent::GetRecommendedRenderTargetSize(uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetRecommendedRenderTargetSize");
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
            *pnWidth = data[0]; *pnHeight = data[1];
        }
    }
    else {
        real_component_->GetRecommendedRenderTargetSize(pnWidth, pnHeight);
    }
}

void RpcDisplayComponent::GetEyeOutputViewport(vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetEyeOutputViewport", RpcValue((int)eEye));
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 4) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
            *pnX = data[0]; *pnY = data[1]; *pnWidth = data[2]; *pnHeight = data[3];
        }
    }
    else {
        real_component_->GetEyeOutputViewport(eEye, pnX, pnY, pnWidth, pnHeight);
    }
}

void RpcDisplayComponent::GetProjectionRaw(vr::EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetProjectionRaw", RpcValue((int)eEye));
        if (result.isPointer() && result.asPointer().second == sizeof(float) * 4) {
            const float* data = reinterpret_cast<const float*>(result.asPointer().first);
            *pfLeft = data[0]; *pfRight = data[1]; *pfTop = data[2]; *pfBottom = data[3];
        }
    }
    else {
        real_component_->GetProjectionRaw(eEye, pfLeft, pfRight, pfTop, pfBottom);
    }
}

vr::DistortionCoordinates_t RpcDisplayComponent::ComputeDistortion(vr::EVREye eEye, float fU, float fV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "ComputeDistortion", RpcValue((int)eEye), RpcValue(fU), RpcValue(fV));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::DistortionCoordinates_t)) {
            return *reinterpret_cast<const vr::DistortionCoordinates_t*>(result.asPointer().first);
        }
        return vr::DistortionCoordinates_t();
    }
    else {
        return real_component_->ComputeDistortion(eEye, fU, fV);
    }
}

bool RpcDisplayComponent::ComputeInverseDistortion(vr::HmdVector2_t *pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "ComputeInverseDistortion", RpcValue((int)eEye), RpcValue((int)unChannel), RpcValue(fU), RpcValue(fV));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::HmdVector2_t)) {
            if (pResult) {
                *pResult = *reinterpret_cast<const vr::HmdVector2_t*>(result.asPointer().first);
            }
            return true;
        }
        return false;
    }
    else {
        return real_component_->ComputeInverseDistortion(pResult, eEye, unChannel, fU, fV);
    }
}
