#pragma once

#include "driver_log.h"
#include "driver_host.h"
#include "driver_input.h"
#include "driver_manager.h"
#include "properties.h"
#include "resources.h"
#include "settings.h"

#include <openvr_driver.h>

namespace ignition {

  class DriverContext : public vr::IVRDriverContext {
  public:
    /** IVRDriverContext **/

    void *GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError = nullptr) override;
    vr::DriverHandle_t GetDriverHandle() override;

  private:
    DriverLog m_driverLog = {};
    DriverHost m_driverHost = {};
    DriverInput m_driverInput = {};
    DriverManager m_driverManager = {};
    Properties m_properties = {};
    Resources m_resources = {};
    Settings m_settings = {};
  };

}
