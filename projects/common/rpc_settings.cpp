#include "rpc_interfaces.h"
#include <vector>

// --- RpcSettings ---

RpcSettings::RpcSettings(vr::IVRSettings* real) : RpcObject(), real_settings_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_Settings_GetBool, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            bool result = this->GetBool(section.c_str(), key.c_str(), &err);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&result, (char*)&result + sizeof(result));
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_Settings_GetInt32, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            int32_t result = this->GetInt32(section.c_str(), key.c_str(), &err);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&result, (char*)&result + sizeof(result));
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_Settings_GetFloat, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            float result = this->GetFloat(section.c_str(), key.c_str(), &err);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&result, (char*)&result + sizeof(result));
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return RpcValue(buffer.data(), buffer.size());
        });

        this->RegisterFunction(RPCFunction_Settings_GetString, [this](const auto& args) {
            char buffer[vr::k_unMaxPropertyStringSize];
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->GetString(section.c_str(), key.c_str(), buffer, sizeof(buffer), &err);
            
            std::vector<char> return_buffer;
            std::string result_str(buffer);
            return_buffer.insert(return_buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return_buffer.insert(return_buffer.end(), result_str.begin(), result_str.end());
            return RpcValue(return_buffer.data(), return_buffer.size());
        });

        this->RegisterFunction(RPCFunction_Settings_SetBool, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->SetBool(section.c_str(), key.c_str(), args[2].asInt(), &err);
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_Settings_SetInt32, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->SetInt32(section.c_str(), key.c_str(), args[2].asInt(), &err);
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_Settings_SetFloat, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->SetFloat(section.c_str(), key.c_str(), args[2].asFloat(), &err);
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_Settings_SetString, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            const std::string value = args[2].asString();
            this->SetString(section.c_str(), key.c_str(), value.c_str(), &err);
            return RpcValue((int)err);
        });

        this->RegisterFunction(RPCFunction_Settings_RemoveSection, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            this->RemoveSection(section.c_str(), &err);
            return RpcValue((int)err);
        });
        
        this->RegisterFunction(RPCFunction_Settings_RemoveKeyInSection, [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->RemoveKeyInSection(section.c_str(), key.c_str(), &err);
            return RpcValue((int)err);
        });
    }
}

RpcSettings::RpcSettings(RpcObjectId id) : RpcObject(id) {}

RpcSettings::~RpcSettings() {
    error_name_cache_.clear();
}

RpcClassEnum RpcSettings::GetRpcClassId() const {
    return RPCClassSettings;
}

const char *RpcSettings::GetSettingsErrorNameFromEnum(vr::EVRSettingsError eError) {
    if (IsProxy()) {
        // This function is problematic because it needs to return a const char * that lives on.
        // So we need a cache of error names, and we'll ask the real implementation for any we don't have yet.
        auto it = error_name_cache_.find(eError);
        if (it != error_name_cache_.end()) {
            return it->second.c_str();
        }
        
        // We don't have it. Ask the remote side.
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_GetSettingsErrorNameFromEnum, RpcValue((int)eError));
        std::string name = result.asString();
        error_name_cache_[eError] = name;
        return error_name_cache_[eError].c_str();
    }
    else {
        return real_settings_->GetSettingsErrorNameFromEnum(eError);
    }
}

void RpcSettings::SetBool(const char *pchSection, const char *pchSettingsKey, bool bValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_SetBool, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue((int)bValue));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    }
    else {
        real_settings_->SetBool(pchSection, pchSettingsKey, bValue, peError);
    }
}

void RpcSettings::SetInt32(const char *pchSection, const char *pchSettingsKey, int32_t nValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_SetInt32, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue(nValue));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    }
    else {
        real_settings_->SetInt32(pchSection, pchSettingsKey, nValue, peError);
    }
}

void RpcSettings::SetFloat(const char *pchSection, const char *pchSettingsKey, float flValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_SetFloat, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue(flValue));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    }
    else {
        real_settings_->SetFloat(pchSection, pchSettingsKey, flValue, peError);
    }
}

void RpcSettings::SetString(const char *pchSection, const char *pchSettingsKey, const char *pchValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_SetString, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue(std::string(pchValue)));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    }
    else {
        real_settings_->SetString(pchSection, pchSettingsKey, pchValue, peError);
    }
}

bool RpcSettings::GetBool(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_GetBool, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer() && result_val.asPointer().second == sizeof(bool) + sizeof(vr::EVRSettingsError)) {
            const char* ptr = result_val.asPointer().first;
            bool result = *reinterpret_cast<const bool*>(ptr);
            if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(ptr + sizeof(bool));
            return result;
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        return false;
    }
    else {
        return real_settings_->GetBool(pchSection, pchSettingsKey, peError);
    }
}

int32_t RpcSettings::GetInt32(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_GetInt32, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer() && result_val.asPointer().second == sizeof(int32_t) + sizeof(vr::EVRSettingsError)) {
            const char* ptr = result_val.asPointer().first;
            int32_t result = *reinterpret_cast<const int32_t*>(ptr);
            if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(ptr + sizeof(int32_t));
            return result;
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        return 0;
    }
    else {
        return real_settings_->GetInt32(pchSection, pchSettingsKey, peError);
    }
}

float RpcSettings::GetFloat(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_GetFloat, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer() && result_val.asPointer().second == sizeof(float) + sizeof(vr::EVRSettingsError)) {
            const char* ptr = result_val.asPointer().first;
            float result = *reinterpret_cast<const float*>(ptr);
            if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(ptr + sizeof(float));
            return result;
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        return 0.0f;
    }
    else {
        return real_settings_->GetFloat(pchSection, pchSettingsKey, peError);
    }
}

void RpcSettings::GetString(const char *pchSection, const char *pchSettingsKey, VR_OUT_STRING() char *pchValue, uint32_t unValueLen, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_GetString, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer()) {
            auto data = result_val.asPointer();
            if (data.second >= sizeof(vr::EVRSettingsError)) {
                if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(data.first);
                std::string result_str(data.first + sizeof(vr::EVRSettingsError), data.second - sizeof(vr::EVRSettingsError));
                if (pchValue && unValueLen > 0) {
                    memcpy(pchValue, result_str.c_str(), std::min((size_t)unValueLen - 1, result_str.size()));
                    pchValue[std::min((size_t)unValueLen - 1, result_str.size())] = '\0';
                }
                return;
            }
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        if (unValueLen > 0) pchValue[0] = '\0';
    } 
    else {
        real_settings_->GetString(pchSection, pchSettingsKey, pchValue, unValueLen, peError);
    }
}

void RpcSettings::RemoveSection(const char *pchSection, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_RemoveSection, RpcValue(std::string(pchSection)));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    }
    else {
        real_settings_->RemoveSection(pchSection, peError);
    }
}

void RpcSettings::RemoveKeyInSection(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Settings_RemoveKeyInSection, RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    }
    else {
        real_settings_->RemoveKeyInSection(pchSection, pchSettingsKey, peError);
    }
}
