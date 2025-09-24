#pragma once

#include <openvr.hpp>

#ifdef _WIN32
#define HMD_DLL_EXPORT extern "C" __declspec(dllexport)
#else
#define HMD_DLL_EXPORT extern "C" __attribute__((visibility("default")))
#endif

HMD_DLL_EXPORT
void *HmdDriverFactory(const char *pInterfaceName, int *pReturnCode);
