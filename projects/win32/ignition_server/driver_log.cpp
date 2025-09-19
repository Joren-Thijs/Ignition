#include "driver_log.h"

#include <cstdio>

namespace ignition {

  void DriverLog::Log(const char *pchLogMessage) {
    printf("%s\n", pchLogMessage);
  }

}
