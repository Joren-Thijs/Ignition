#pragma once

#include <openvr.hpp>

namespace ignition
{

	class DriverInput : public vr::IVRDriverInput
	{
	public:
		/** IVRDriverInput **/

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
	};

}
