#include "driver_input.h"

namespace ignition {

	vr::EVRInputError DriverInput::CreateBooleanComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::UpdateBooleanComponent(vr::VRInputComponentHandle_t ulComponent, bool bNewValue, double fTimeOffset) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::CreateScalarComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle, vr::EVRScalarType eType, vr::EVRScalarUnits eUnits) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::UpdateScalarComponent(vr::VRInputComponentHandle_t ulComponent, float fNewValue, double fTimeOffset) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::CreateHapticComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::CreateSkeletonComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, const char *pchSkeletonPath, const char *pchBasePosePath, vr::EVRSkeletalTrackingLevel eSkeletalTrackingLevel, const vr::VRBoneTransform_t *pGripLimitTransforms, uint32_t unGripLimitTransformCount, vr::VRInputComponentHandle_t *pHandle) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::UpdateSkeletonComponent(vr::VRInputComponentHandle_t ulComponent, vr::EVRSkeletalMotionRange eMotionRange, const vr::VRBoneTransform_t *pTransforms, uint32_t unTransformCount) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::CreatePoseComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::UpdatePoseComponent(vr::VRInputComponentHandle_t ulComponent, const vr::HmdMatrix34_t *pMatPoseOffset, double fTimeOffset) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::CreateEyeTrackingComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
		return vr::VRInputError_None;
	}
	vr::EVRInputError DriverInput::UpdateEyeTrackingComponent(vr::VRInputComponentHandle_t ulComponent, const vr::VREyeTrackingData_t *pEyeTrackingData, double fTimeOffset) {
		return vr::VRInputError_None;
	}

}
