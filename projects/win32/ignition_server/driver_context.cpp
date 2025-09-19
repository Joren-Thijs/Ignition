#include "driver_context.h"

#include <cstdio>

namespace ignition {

  void *DriverContext::GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError) {

    if (strcmp(vr::IVRDriverLog_Version, pchInterfaceVersion) == 0) {
      return &m_driverLog;
    }
    if (strcmp(vr::IVRServerDriverHost_Version, pchInterfaceVersion) == 0) {
      return &m_driverHost;
    }
    if (strcmp("IVRDriverInput_003", pchInterfaceVersion) == 0) {
      return &m_driverInput;
    }
    if (strcmp(vr::IVRDriverInput_Version, pchInterfaceVersion) == 0) {
      return &m_driverInput;
    }
    if (strcmp(vr::IVRDriverManager_Version, pchInterfaceVersion) == 0) {
      return &m_driverManager;
    }
    if (strcmp(vr::IVRProperties_Version, pchInterfaceVersion) == 0) {
      return &m_properties;
    }
    if (strcmp(vr::IVRResources_Version, pchInterfaceVersion) == 0) {
      return &m_resources;
    }
    if (strcmp(vr::IVRSettings_Version, pchInterfaceVersion) == 0) {
      return &m_settings;
    }

    printf("DriverContext::GetGenericInterface(%s)\n", pchInterfaceVersion);
    return nullptr;
  }

  vr::DriverHandle_t DriverContext::GetDriverHandle() {
    printf("DriverContext::GetDriverHandle()\n");
    return 0;
  }

}
