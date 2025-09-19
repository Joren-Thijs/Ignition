#include "driver_host.h"

#include <cstdio>

namespace ignition {

  bool DriverHost::TrackedDeviceAdded(const char *pchDeviceSerialNumber, vr::ETrackedDeviceClass eDeviceClass, vr::ITrackedDeviceServerDriver *pDriver) {
    printf("DriverHost::TrackedDeviceAdded(%s, %i)\n", pchDeviceSerialNumber, eDeviceClass);
    pDriver->Activate(0);
    return false;
  }

  void DriverHost::TrackedDevicePoseUpdated(uint32_t unWhichDevice, const vr::DriverPose_t &newPose, uint32_t unPoseStructSize) {
    printf("DriverHost::TrackedDevicePoseUpdated(%i, %i)\n", unWhichDevice, unPoseStructSize);
  }

  void DriverHost::VsyncEvent(double vsyncTimeOffsetSeconds) {
    printf("DriverHost::VsyncEvent()\n");
  }

  void DriverHost::VendorSpecificEvent(uint32_t unWhichDevice, vr::EVREventType eventType, const vr::VREvent_Data_t &eventData, double eventTimeOffset) {
    printf("DriverHost::VendorSpecificEvent()\n");
  }

  bool DriverHost::IsExiting() {
    printf("DriverHost::IsExiting()\n");
    return false;
  }

  bool DriverHost::PollNextEvent(vr::VREvent_t *pEvent, uint32_t uncbVREvent) {
    printf("DriverHost::PollNextEvent()\n");
    return false;
  }

  void DriverHost::GetRawTrackedDevicePoses(float fPredictedSecondsFromNow, vr::TrackedDevicePose_t *pTrackedDevicePoseArray, uint32_t unTrackedDevicePoseArrayCount) {
    printf("DriverHost::GetRawTrackedDevicePoses()\n");
  }

  void DriverHost::RequestRestart(const char *pchLocalizedReason, const char *pchExecutableToStart, const char *pchArguments, const char *pchWorkingDirectory) {
    printf("DriverHost::RequestRestart()\n");
  }

  uint32_t DriverHost::GetFrameTimings(vr::Compositor_FrameTiming *pTiming, uint32_t nFrames) {
    printf("DriverHost::GetFrameTimings()\n");
    return 0;
  }

  void DriverHost::SetDisplayEyeToHead(uint32_t unWhichDevice, const vr::HmdMatrix34_t &eyeToHeadLeft, const vr::HmdMatrix34_t &eyeToHeadRight) {
    printf("DriverHost::SetDisplayEyeToHead()\n");
  }

  void DriverHost::SetDisplayProjectionRaw(uint32_t unWhichDevice, const vr::HmdRect2_t &eyeLeft, const vr::HmdRect2_t &eyeRight) {
    printf("DriverHost::SetDisplayProjectionRaw()\n");
  }

  void DriverHost::SetRecommendedRenderTargetSize(uint32_t unWhichDevice, uint32_t nWidth, uint32_t nHeight) {
    printf("DriverHost::SetRecommendedRenderTargetSize()\n");
  }

}
