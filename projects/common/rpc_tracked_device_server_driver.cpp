#include "vr_rpc_interfaces.h"
#include <cstring>
#include <iostream>

// --- RpcTrackedDeviceServerDriver ---

RpcTrackedDeviceServerDriver::RpcTrackedDeviceServerDriver(vr::ITrackedDeviceServerDriver* real) : RpcObject(), real_driver_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";

        RpcSystem::RegisterFunction(prefix + "Activate", [this](const auto& args) {
            return RpcValue((int)this->Activate(args[0].asInt()));
        });

        RpcSystem::RegisterFunction(prefix + "Deactivate", [this](const auto& args) {
            this->Deactivate();
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "EnterStandby", [this](const auto& args) {
            this->EnterStandby();
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "DebugRequest", [this](const auto& args) {
            char response_buf[vr::k_unMaxDriverDebugResponseSize];
            this->DebugRequest(args[0].asString().c_str(), response_buf, sizeof(response_buf));
            return RpcValue(std::string(response_buf));
        });

        RpcSystem::RegisterFunction(prefix + "GetPose", [this](const auto& args) {
            vr::DriverPose_t pose = this->GetPose();
            return RpcValue((const char*)&pose, sizeof(pose));
        });

        RpcSystem::RegisterFunction(prefix + "GetComponent", [this](const auto& args) {
            auto componentNameStr = args[0].asString();
            const char* componentName = componentNameStr.c_str();
            void* component = this->GetComponent(componentName);
            
            if (!component) {
                return RpcValue(); // Return null RpcValue
            }

            return RpcValue(dynamic_cast<RpcObject*>((RpcObject*)component));
        });
    }
}

RpcTrackedDeviceServerDriver::RpcTrackedDeviceServerDriver(RpcObjectId id) : RpcObject(id)
{
    std::cout << "Initializing RpcTrackedDeviceServerDriver proxy with id " << id << std::endl;
}

RpcTrackedDeviceServerDriver::~RpcTrackedDeviceServerDriver() {}

const std::string& RpcTrackedDeviceServerDriver::GetRpcClassName() const {
    static const std::string name = "ITrackedDeviceServerDriver";
    return name;
}

vr::EVRInitError RpcTrackedDeviceServerDriver::Activate(uint32_t unObjectId) {
    if (IsProxy()) {
        return (vr::EVRInitError)RpcSystem::CallMethod(GetId(), "Activate", RpcValue((int)unObjectId)).asInt();
    }
    else {
        return real_driver_->Activate(unObjectId);
    }
}

void RpcTrackedDeviceServerDriver::Deactivate() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "Deactivate");
    }
    else {
        real_driver_->Deactivate();
    }
}

void RpcTrackedDeviceServerDriver::EnterStandby() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "EnterStandby");
    }
    else {
        real_driver_->EnterStandby();
    }
}

void *RpcTrackedDeviceServerDriver::GetComponent(const char *pchComponentNameAndVersion) {
    if (IsProxy()) { // Client-side proxy
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetComponent", RpcValue(std::string(pchComponentNameAndVersion)));
        if (result.isObject()) {
            RpcObject* obj = result.asObject();
            if (!obj) {
                return nullptr;
            }

            std::string interface_str = pchComponentNameAndVersion;
            if (interface_str == vr::IVRDisplayComponent_Version) {
                return dynamic_cast<vr::IVRDisplayComponent*>(obj);
            } else if (interface_str == vr::IVRCameraComponent_Version) {
                return dynamic_cast<vr::IVRCameraComponent*>(obj);
            }
        }
        return nullptr;
    }
    else { // Server-side real object
        void* component = real_driver_->GetComponent(pchComponentNameAndVersion);
        if (!component) {
            return nullptr;
        }

        RpcObject* rpc_wrapper = nullptr;
        std::string interface_str = pchComponentNameAndVersion;

        if (interface_str == vr::IVRDisplayComponent_Version) {
            rpc_wrapper = new RpcDisplayComponent(static_cast<vr::IVRDisplayComponent*>(component));
        }
        else if (interface_str == vr::IVRCameraComponent_Version) {
            rpc_wrapper = new RpcCameraComponent(static_cast<vr::IVRCameraComponent*>(component));
        }
        else {
            std::cerr << "Warning: GetComponent returned unknown interface " << interface_str << std::endl;
            return nullptr; // Unknown interface
        }

        return rpc_wrapper;
    }
}

void RpcTrackedDeviceServerDriver::DebugRequest(const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize) {
    if (IsProxy()) {
        if (!pchResponseBuffer || unResponseBufferSize == 0) return;
        RpcValue result = RpcSystem::CallMethod(GetId(), "DebugRequest", RpcValue(std::string(pchRequest)));
        std::string response = result.asString();
        strncpy(pchResponseBuffer, response.c_str(), unResponseBufferSize);        
    }
    else {
        real_driver_->DebugRequest(pchRequest, pchResponseBuffer, unResponseBufferSize);
    }
}

vr::DriverPose_t RpcTrackedDeviceServerDriver::GetPose() {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetPose");
        if (result.isPointer() && result.asPointer().second == sizeof(vr::DriverPose_t)) {
            return *reinterpret_cast<const vr::DriverPose_t*>(result.asPointer().first);
        }
        return vr::DriverPose_t();
    }
    else {
        return real_driver_->GetPose();
    }
}
