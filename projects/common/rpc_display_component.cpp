#include "rpc_interfaces.h"

// --- RpcDisplayComponent ---

RpcDisplayComponent::RpcDisplayComponent(vr::IVRDisplayComponent* real) : RpcObject(), real_component_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_DisplayComponent_GetWindowBounds, [this](const auto& args) {
            int32_t x, y;
            uint32_t w, h;
            this->GetWindowBounds(&x, &y, &w, &h);
            int32_t data[] = {x, y, (int32_t)w, (int32_t)h};
            return RpcValue((const char*)data, sizeof(data));
        });

        this->RegisterFunction(RPCFunction_DisplayComponent_IsDisplayOnDesktop, [this](const auto& args) {
            return RpcValue((int)this->IsDisplayOnDesktop());
        });
        
        this->RegisterFunction(RPCFunction_DisplayComponent_IsDisplayRealDisplay, [this](const auto& args) {
            return RpcValue((int)this->IsDisplayRealDisplay());
        });

        this->RegisterFunction(RPCFunction_DisplayComponent_GetRecommendedRenderTargetSize, [this](const auto& args) {
            uint32_t w, h;
            this->GetRecommendedRenderTargetSize(&w, &h);
            uint32_t data[] = {w, h};
            return RpcValue((const char*)data, sizeof(data));
        });

        this->RegisterFunction(RPCFunction_DisplayComponent_GetEyeOutputViewport, [this](const auto& args) {
            uint32_t x, y, w, h;
            this->GetEyeOutputViewport((vr::EVREye)args[0].asInt(), &x, &y, &w, &h);
            uint32_t data[] = {x, y, w, h};
            return RpcValue((const char*)data, sizeof(data));
        });

        this->RegisterFunction(RPCFunction_DisplayComponent_GetProjectionRaw, [this](const auto& args) {
            float l, r, t, b;
            this->GetProjectionRaw((vr::EVREye)args[0].asInt(), &l, &r, &t, &b);
            float data[] = {l, r, t, b};
            return RpcValue((const char*)data, sizeof(data));
        });

        this->RegisterFunction(RPCFunction_DisplayComponent_ComputeDistortion, [this](const auto& args) {
            vr::DistortionCoordinates_t coords = this->ComputeDistortion((vr::EVREye)args[0].asInt(), args[1].asFloat(), args[2].asFloat());
            return RpcValue((const char*)&coords, sizeof(coords));
        });
        
        this->RegisterFunction(RPCFunction_DisplayComponent_ComputeInverseDistortion, [this](const auto& args) {
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

RpcClassEnum RpcDisplayComponent::GetRpcClassId() const {
    return RPCClassDisplayComponent;
}

void RpcDisplayComponent::GetWindowBounds(int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_GetWindowBounds);
        if (result.isByteArray() && result.asByteArray().size() == sizeof(int32_t) * 4) {
            const int32_t* data = reinterpret_cast<const int32_t*>(result.asByteArray().data());
            *pnX = data[0]; *pnY = data[1]; *pnWidth = data[2]; *pnHeight = data[3];
        }
    }
    else {
        real_component_->GetWindowBounds(pnX, pnY, pnWidth, pnHeight);
    }
}

bool RpcDisplayComponent::IsDisplayOnDesktop() {
    return IsProxy() ?
        RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_IsDisplayOnDesktop).asInt() :
        real_component_->IsDisplayOnDesktop();
}

bool RpcDisplayComponent::IsDisplayRealDisplay() {
    return IsProxy() ? 
        RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_IsDisplayRealDisplay).asInt() :
        real_component_->IsDisplayRealDisplay();
}

void RpcDisplayComponent::GetRecommendedRenderTargetSize(uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_GetRecommendedRenderTargetSize);
        if (result.isByteArray() && result.asByteArray().size() == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asByteArray().data());
            *pnWidth = data[0]; *pnHeight = data[1];
        }
    }
    else {
        real_component_->GetRecommendedRenderTargetSize(pnWidth, pnHeight);
    }
}

void RpcDisplayComponent::GetEyeOutputViewport(vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_GetEyeOutputViewport, RpcValue((int)eEye));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(uint32_t) * 4) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asByteArray().data());
            *pnX = data[0]; *pnY = data[1]; *pnWidth = data[2]; *pnHeight = data[3];
        }
    }
    else {
        real_component_->GetEyeOutputViewport(eEye, pnX, pnY, pnWidth, pnHeight);
    }
}

void RpcDisplayComponent::GetProjectionRaw(vr::EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_GetProjectionRaw, RpcValue((int)eEye));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(float) * 4) {
            const float* data = reinterpret_cast<const float*>(result.asByteArray().data());
            *pfLeft = data[0]; *pfRight = data[1]; *pfTop = data[2]; *pfBottom = data[3];
        }
    }
    else {
        real_component_->GetProjectionRaw(eEye, pfLeft, pfRight, pfTop, pfBottom);
    }
}

vr::DistortionCoordinates_t RpcDisplayComponent::ComputeDistortion(vr::EVREye eEye, float fU, float fV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_ComputeDistortion, RpcValue((int)eEye), RpcValue(fU), RpcValue(fV));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(vr::DistortionCoordinates_t)) {
            return *reinterpret_cast<const vr::DistortionCoordinates_t*>(result.asByteArray().data());
        }
        return vr::DistortionCoordinates_t();
    }
    else {
#ifdef __MINGW64__
        // MinGW cross-compile ABI fix for returning structs by value.
        // The MSVC ABI for returning a struct of this size is to pass a hidden pointer
        // as the first argument. MinGW GCC doesn't do this by default for virtual calls
        // across DLL boundaries built with different compilers, leading to a crash.
        // We work around this by getting the function pointer from the vtable and
        // calling it with the correct signature using the ms_abi attribute.

        // Define the function pointer type with the Microsoft ABI.
        // 1st param: 'this' pointer. 2nd param: hidden pointer to the return struct.
        using ComputeDistortion_ms_abi = void (__attribute__((ms_abi)) *)(void*, vr::DistortionCoordinates_t*, vr::EVREye, float, float);

        // Get the vtable from the object instance.
        void** vtable = *(void***)real_component_;

        // Get the function pointer. ComputeDistortion is the 7th virtual function (index 6).
        auto func = (ComputeDistortion_ms_abi)vtable[6];

        vr::DistortionCoordinates_t coords;
        func(real_component_, &coords, eEye, fU, fV);
        return coords;
#else
        return real_component_->ComputeDistortion(eEye, fU, fV);
#endif
    }
}

bool RpcDisplayComponent::ComputeInverseDistortion(vr::HmdVector2_t *pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DisplayComponent_ComputeInverseDistortion, RpcValue((int)eEye), RpcValue((int)unChannel), RpcValue(fU), RpcValue(fV));
        if (result.isByteArray() && result.asByteArray().size() == sizeof(vr::HmdVector2_t)) {
            if (pResult) {
                *pResult = *reinterpret_cast<const vr::HmdVector2_t*>(result.asByteArray().data());
            }
            return true;
        }
        return false;
    }
    else {
        return real_component_->ComputeInverseDistortion(pResult, eEye, unChannel, fU, fV);
    }
}
