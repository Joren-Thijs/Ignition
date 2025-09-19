#include "properties.h"

#include <cstdio>

namespace ignition {

  vr::ETrackedPropertyError Properties::ReadPropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyRead_t *pBatch, uint32_t unBatchEntryCount) {
    printf("Properties::ReadPropertyBatch()\n");
    return vr::TrackedProp_Success;
  }
  vr::ETrackedPropertyError Properties::WritePropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyWrite_t *pBatch, uint32_t unBatchEntryCount) {
    printf("Properties::WritePropertyBatch()\n");
    return vr::TrackedProp_Success;
  }
  const char *Properties::GetPropErrorNameFromEnum(vr::ETrackedPropertyError error) {
    printf("Properties::GetPropErrorNameFromEnum()\n");
    return nullptr;
  }
  vr::PropertyContainerHandle_t Properties::TrackedDeviceToPropertyContainer(vr::TrackedDeviceIndex_t nDevice) {
    printf("Properties::TrackedDeviceToPropertyContainer()\n");
    return 0;
  }

}

