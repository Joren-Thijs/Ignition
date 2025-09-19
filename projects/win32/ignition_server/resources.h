#pragma once

#include <openvr.hpp>

namespace ignition
{

  class Resources : public vr::IVRResources
  {
  public:
    /** IVRResources **/

    uint32_t LoadSharedResource(const char *pchResourceName, char *pchBuffer, uint32_t unBufferLen) override;
    uint32_t GetResourceFullPath(const char *pchResourceName, const char *pchResourceTypeDirectory, VR_OUT_STRING() char *pchPathBuffer, uint32_t unBufferLen) override;
  };

}
