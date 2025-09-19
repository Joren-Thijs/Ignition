#pragma once

#include <openvr.hpp>

namespace ignition
{

  class DriverManager : public vr::IVRDriverManager
  {
  public:
    /** IVRDriverManager **/

    uint32_t GetDriverCount() const override;
    uint32_t GetDriverName(vr::DriverId_t nDriver, VR_OUT_STRING() char *pchValue, uint32_t unBufferSize) override;
    vr::DriverHandle_t GetDriverHandle(const char *pchDriverName) override;
    bool IsEnabled(vr::DriverId_t nDriver) const override;
  };

}
