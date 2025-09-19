#include "driver_manager.h"

#include <cstdio>

namespace ignition {

  uint32_t DriverManager::GetDriverCount() const {
    printf("DriverManager::GetDriverCount()\n");
    return 1;
  }
  uint32_t DriverManager::GetDriverName(vr::DriverId_t nDriver, char *pchValue, uint32_t unBufferSize) {
    printf("DriverManager::GetDriverName()\n");
    return 0;
  }
  vr::DriverHandle_t DriverManager::GetDriverHandle(const char *pchDriverName) {
    printf("DriverManager::GetDriverHandle()\n");
    return 0;
  }
  bool DriverManager::IsEnabled(vr::DriverId_t nDriver) const {
    printf("DriverManager::IsEnabled()\n");
    return true;
  }

}

