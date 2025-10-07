#include "vr_rpc_interfaces.h"
#include <vector>

// --- RpcDriverInput ---

RpcDriverInput::RpcDriverInput(vr::IVRDriverInput* real) : RpcObject(), real_input_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_DriverInput_CreateBooleanComponent, [this](const auto& args) {
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->CreateBooleanComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_DriverInput_UpdateBooleanComponent, [this](const auto& args) {
            vr::EVRInputError err = this->UpdateBooleanComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), args[1].asInt(), args[2].asDouble());
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_DriverInput_CreateScalarComponent, [this](const auto& args) {
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->CreateScalarComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle, (vr::EVRScalarType)args[2].asInt(), (vr::EVRScalarUnits)args[3].asInt());
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_DriverInput_UpdateScalarComponent, [this](const auto& args) {
            vr::EVRInputError err = this->UpdateScalarComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), args[1].asFloat(), args[2].asDouble());
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_DriverInput_CreateHapticComponent, [this](const auto& args) {
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->CreateHapticComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_DriverInput_CreateSkeletonComponent, [this](const auto& args) {
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string name_str = args[1].asString();
            const std::string skeleton_path_str = args[2].asString();
            const std::string base_pose_path_str = args[3].asString();
            auto grip_transforms_data = args[5].asPointer();
            vr::EVRInputError err = this->CreateSkeletonComponent(
                (vr::PropertyContainerHandle_t)args[0].asUint64(),
                name_str.c_str(),
                skeleton_path_str.c_str(),
                base_pose_path_str.c_str(),
                (vr::EVRSkeletalTrackingLevel)args[4].asInt(),
                (const vr::VRBoneTransform_t *)grip_transforms_data.first,
                (uint32_t)args[6].asInt(), &handle );
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_DriverInput_UpdateSkeletonComponent, [this](const auto& args) {
            auto transforms_data = args[2].asPointer();
            vr::EVRInputError err = this->UpdateSkeletonComponent( (vr::VRInputComponentHandle_t)args[0].asUint64(), (vr::EVRSkeletalMotionRange)args[1].asInt(), (const vr::VRBoneTransform_t *)transforms_data.first, (uint32_t)args[3].asInt() );
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_DriverInput_CreatePoseComponent, [this](const auto& args) {
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->CreatePoseComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_DriverInput_UpdatePoseComponent, [this](const auto& args) {
            vr::EVRInputError err = this->UpdatePoseComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), (const vr::HmdMatrix34_t*)args[1].asPointer().first, args[2].asDouble());
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_DriverInput_CreateEyeTrackingComponent, [this](const auto& args) {
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->CreateEyeTrackingComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });
        
        this->RegisterFunction(RPCFunction_DriverInput_UpdateEyeTrackingComponent, [this](const auto& args) {
            const auto& arg = args[1].asPointer();
            if(arg.second != sizeof(vr::VREyeTrackingData_t)) {
                return RpcValue((int)vr::VRInputError_InvalidParam);
            }
            vr::EVRInputError err = this->UpdateEyeTrackingComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), (const vr::VREyeTrackingData_t*)arg.first, args[2].asDouble());
            return RpcValue((int)err);
        });
    }
}
RpcDriverInput::RpcDriverInput(RpcObjectId id) : RpcObject(id) {}
RpcDriverInput::~RpcDriverInput() {}

RpcClassEnum RpcDriverInput::GetRpcClassId() const {
    return Class_DriverInput;
}

vr::EVRInputError RpcDriverInput::CreateBooleanComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_CreateBooleanComponent, RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
    }
    else {
        return real_input_->CreateBooleanComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdateBooleanComponent(vr::VRInputComponentHandle_t ulComponent, bool bNewValue, double fTimeOffset) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_UpdateBooleanComponent, RpcValue(ulComponent), RpcValue((int)bNewValue), RpcValue(fTimeOffset)).asInt();
    }
    else {
        return real_input_->UpdateBooleanComponent(ulComponent, bNewValue, fTimeOffset);
    }
}

vr::EVRInputError RpcDriverInput::CreateScalarComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle, vr::EVRScalarType eType, vr::EVRScalarUnits eUnits) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_CreateScalarComponent, RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)), RpcValue((int)eType), RpcValue((int)eUnits));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
    }
    else {
        return real_input_->CreateScalarComponent(ulContainer, pchName, pHandle, eType, eUnits);
    }
}

vr::EVRInputError RpcDriverInput::UpdateScalarComponent(vr::VRInputComponentHandle_t ulComponent, float fNewValue, double fTimeOffset) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_UpdateScalarComponent, RpcValue(ulComponent), RpcValue(fNewValue), RpcValue(fTimeOffset)).asInt();
    }
    else {
        return real_input_->UpdateScalarComponent(ulComponent, fNewValue, fTimeOffset);
    }
}

vr::EVRInputError RpcDriverInput::CreateHapticComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_CreateHapticComponent, RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
     }
     else {
        return real_input_->CreateHapticComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::CreateSkeletonComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, const char *pchSkeletonPath, const char *pchBasePosePath, vr::EVRSkeletalTrackingLevel eSkeletalTrackingLevel, const vr::VRBoneTransform_t *pGripLimitTransforms, uint32_t unGripLimitTransformCount, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_CreateSkeletonComponent,
            RpcValue((uint64_t)ulContainer),
            RpcValue(std::string(pchName)),
            RpcValue(std::string(pchSkeletonPath)),
            RpcValue(std::string(pchBasePosePath)),
            RpcValue((int)eSkeletalTrackingLevel),
            RpcValue((const char*)pGripLimitTransforms, unGripLimitTransformCount * sizeof(vr::VRBoneTransform_t)),
            RpcValue((int)unGripLimitTransformCount)
        );
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
    }
    else {
        return real_input_->CreateSkeletonComponent(ulContainer, pchName, pchSkeletonPath, pchBasePosePath, eSkeletalTrackingLevel, pGripLimitTransforms, unGripLimitTransformCount, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdateSkeletonComponent(vr::VRInputComponentHandle_t ulComponent, vr::EVRSkeletalMotionRange eMotionRange, const vr::VRBoneTransform_t *pTransforms, uint32_t unTransformCount) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_UpdateSkeletonComponent,
            RpcValue(ulComponent),
            RpcValue((int)eMotionRange),
            RpcValue((const char*)pTransforms, unTransformCount * sizeof(vr::VRBoneTransform_t)),
            RpcValue((int)unTransformCount)
        ).asInt();
    }
    else {
        return real_input_->UpdateSkeletonComponent(ulComponent, eMotionRange, pTransforms, unTransformCount);
    }
}

vr::EVRInputError RpcDriverInput::CreatePoseComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_CreatePoseComponent, RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
     }
     else {
        return real_input_->CreatePoseComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdatePoseComponent(vr::VRInputComponentHandle_t ulComponent, const vr::HmdMatrix34_t *pMatPoseOffset, double fTimeOffset) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_UpdatePoseComponent, RpcValue(ulComponent), RpcValue((const char*)pMatPoseOffset, sizeof(vr::HmdMatrix34_t)), RpcValue(fTimeOffset)).asInt();
    }
    else {
        return real_input_->UpdatePoseComponent(ulComponent, pMatPoseOffset, fTimeOffset);
    }
}

vr::EVRInputError RpcDriverInput::CreateEyeTrackingComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_CreateEyeTrackingComponent, RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
    }
    else {
        return real_input_->CreateEyeTrackingComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdateEyeTrackingComponent(vr::VRInputComponentHandle_t ulComponent, const vr::VREyeTrackingData_t *pEyeTrackingData, double fTimeOffset) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), RPCFunction_DriverInput_UpdateEyeTrackingComponent, RpcValue(ulComponent), RpcValue((const char*)pEyeTrackingData, sizeof(vr::VREyeTrackingData_t)), RpcValue(fTimeOffset)).asInt();
    }
    else {
        return real_input_->UpdateEyeTrackingComponent(ulComponent, pEyeTrackingData, fTimeOffset);
    }
}
