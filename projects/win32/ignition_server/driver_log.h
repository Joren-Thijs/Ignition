#pragma once

#include <openvr.hpp>

namespace ignition
{

  class DriverLog : public vr::IVRDriverLog
  {
  public:
    /** IVRDriverLog **/

    void Log(const char *pchLogMessage) override;
  };

}
