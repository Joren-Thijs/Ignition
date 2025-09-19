#include "resources.h"

#include <cstdio>

namespace ignition {

  uint32_t Resources::LoadSharedResource(const char *pchResourceName, char *pchBuffer, uint32_t unBufferLen) {
    printf("Resources::LoadSharedResource()\n");
    return 0;
  }
  uint32_t Resources::GetResourceFullPath(const char *pchResourceName, const char *pchResourceTypeDirectory, char *pchPathBuffer, uint32_t unBufferLen) {
    printf("Resources::GetResourceFullPath()\n");
    return 0;
  }

}

