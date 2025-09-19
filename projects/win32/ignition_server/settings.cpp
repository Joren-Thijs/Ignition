#include "settings.h"

namespace ignition {

  const char *Settings::GetSettingsErrorNameFromEnum(vr::EVRSettingsError eError) {
    return nullptr;
  }
  void Settings::SetBool(const char *pchSection, const char *pchSettingsKey, bool bValue, vr::EVRSettingsError *peError) {

  }
  void Settings::SetInt32(const char *pchSection, const char *pchSettingsKey, int32_t nValue, vr::EVRSettingsError *peError) {

  }
  void Settings::SetFloat(const char *pchSection, const char *pchSettingsKey, float flValue, vr::EVRSettingsError *peError) {

  }
  void Settings::SetString(const char *pchSection, const char *pchSettingsKey, const char *pchValue, vr::EVRSettingsError *peError) {

  }
  bool Settings::GetBool(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    return false;
  }
  int32_t Settings::GetInt32(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    return 0;
  }
  float Settings::GetFloat(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    return 1.0f;
  }
  void Settings::GetString(const char *pchSection, const char *pchSettingsKey, char *pchValue, uint32_t unValueLen, vr::EVRSettingsError *peError) {

  }
  void Settings::RemoveSection(const char *pchSection, vr::EVRSettingsError *peError) {

  }
  void Settings::RemoveKeyInSection(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {

  }

}
