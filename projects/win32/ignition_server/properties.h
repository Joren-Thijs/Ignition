#pragma once

#include <openvr.hpp>

namespace ignition
{

  class Properties : public vr::IVRProperties
  {
  public:
    /** IVRProperties **/

    vr::ETrackedPropertyError ReadPropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyRead_t *pBatch, uint32_t unBatchEntryCount) override;
    vr::ETrackedPropertyError WritePropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyWrite_t *pBatch, uint32_t unBatchEntryCount) override;
    const char *GetPropErrorNameFromEnum(vr::ETrackedPropertyError error) override;
    vr::PropertyContainerHandle_t TrackedDeviceToPropertyContainer(vr::TrackedDeviceIndex_t nDevice) override;
  };

}
