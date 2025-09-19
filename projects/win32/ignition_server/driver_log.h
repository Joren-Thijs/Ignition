#pragma once

#include <openvr_driver.h>

namespace ignition {

  class DriverLog : public vr::IVRDriverLog {
  public:
    /** IVRDriverLog **/

    void Log(const char *pchLogMessage) override;
  };

}
