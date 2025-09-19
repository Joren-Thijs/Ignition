#pragma once

#include <openvr.hpp>

namespace ignition
{

  class Settings : public vr::IVRSettings
  {
  public:
    /** IVRSettings **/

    const char *GetSettingsErrorNameFromEnum(vr::EVRSettingsError eError) override;
    void SetBool(const char *pchSection, const char *pchSettingsKey, bool bValue, vr::EVRSettingsError *peError = nullptr) override;
    void SetInt32(const char *pchSection, const char *pchSettingsKey, int32_t nValue, vr::EVRSettingsError *peError = nullptr) override;
    void SetFloat(const char *pchSection, const char *pchSettingsKey, float flValue, vr::EVRSettingsError *peError = nullptr) override;
    void SetString(const char *pchSection, const char *pchSettingsKey, const char *pchValue, vr::EVRSettingsError *peError = nullptr) override;
    bool GetBool(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;
    int32_t GetInt32(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;
    float GetFloat(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;
    void GetString(const char *pchSection, const char *pchSettingsKey, VR_OUT_STRING() char *pchValue, uint32_t unValueLen, vr::EVRSettingsError *peError = nullptr) override;
    void RemoveSection(const char *pchSection, vr::EVRSettingsError *peError = nullptr) override;
    void RemoveKeyInSection(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError = nullptr) override;
  };

}
