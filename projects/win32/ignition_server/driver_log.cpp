#include "driver_log.h"

#include <cstdio>

namespace ignition {

  void DriverLog::Log(const char *pchLogMessage) {
    int logLen = strlen(pchLogMessage);
    bool hasNewline = pchLogMessage[logLen-1] == '\n';
    printf("%s%s", pchLogMessage, hasNewline ? "" : "\n");
  }

}
