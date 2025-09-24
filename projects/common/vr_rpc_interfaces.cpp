#include "vr_rpc_interfaces.h"
#include <stdexcept>


// --- ClientContextManager ---

ClientContextManager::ClientContextManager(vr::IVRDriverContext *real_context) : RpcObject(), real_context_(real_context)
{
    if (!IsProxy()) { // This is the real object on the client side
        RpcSystem::RegisterFunction(std::to_string(GetId()) + ".GetGenericInterface", [this](const auto& args){
            vr::EVRInitError err;
            return RpcValue(static_cast<RpcObject*>(this->GetGenericInterface(args[0].asString().c_str(), &err)));
        });
        RpcSystem::RegisterFunction(std::to_string(GetId()) + ".GetDriverHandle", [this](const auto& args) {
            // vr::DriverHandle_t is a uint64_t
            return RpcValue(this->GetDriverHandle());
        });
    }
}

ClientContextManager::ClientContextManager(RpcObjectId id) : RpcObject(id) {}

ClientContextManager::~ClientContextManager() {
    // Clean up cached wrappers
    for(auto const& [key, val] : interface_cache_) {
        delete val;
    }
}

const std::string& ClientContextManager::GetRpcClassName() const {
    static const std::string name = "ClientContextManager";
    return name;
}

void* ClientContextManager::GetGenericInterface(const char *pchInterfaceVersion, vr::EVRInitError *peError) {
    if (IsProxy()) {
        std::cout << "ClientContextManager: Getting interface " << pchInterfaceVersion << std::endl;

        // Server-side proxy implementation
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetGenericInterface", RpcValue(std::string(pchInterfaceVersion)));
        if (result.isObject()) {
            std::cout << "ClientContextManager: Id: " << result.asObject()->GetId() << std::endl;
            RpcObject* obj = result.asObject();
            if (!obj) {
                if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;
                return nullptr;
            }

            // The user is right, casting to void* and then back is dangerous with multiple inheritance.
            // We must cast to the correct interface type here to ensure the vtable is correct.
            // dynamic_cast is the safe way to do this.
            std::string interface_str = pchInterfaceVersion;
            if (interface_str == vr::IVRServerDriverHost_Version) {
                return dynamic_cast<vr::IVRServerDriverHost*>(obj);
            } else if (interface_str == vr::IVRDriverLog_Version) {
                return dynamic_cast<vr::IVRDriverLog*>(obj);
            } else if (interface_str == vr::IVRSettings_Version) {
                return dynamic_cast<vr::IVRSettings*>(obj);
            } else if (interface_str == "IVRDriverInput_003" || interface_str == vr::IVRDriverInput_Version) {
                return dynamic_cast<vr::IVRDriverInput*>(obj);
            } else if (interface_str == vr::IVRDriverManager_Version) {
                return dynamic_cast<vr::IVRDriverManager*>(obj);
            } else if (interface_str == vr::IVRProperties_Version) {
                return dynamic_cast<vr::IVRProperties*>(obj);
            } else if (interface_str == vr::IVRResources_Version) {
                return dynamic_cast<vr::IVRResources*>(obj);
            }

            // If we don't know the interface, we can't safely cast it.
            if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;
            return nullptr;
        } else {
            if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;
            return nullptr;
        }
    } else {
        // Client-side real implementation
        if (interface_cache_.count(pchInterfaceVersion)) {
            if (peError) *peError = vr::VRInitError_None;
            return interface_cache_.at(pchInterfaceVersion);
        }

        vr::EVRInitError err;
        void* real_interface = real_context_->GetGenericInterface(pchInterfaceVersion, &err);

        std::cout << "ClientContextManager: Wrapping interface " << pchInterfaceVersion << std::endl;

        if (peError) *peError = err;

        if (!real_interface || err != vr::VRInitError_None) {
            return nullptr;
        }

        RpcObject* rpc_wrapper = nullptr;
        std::string interface_str = pchInterfaceVersion;

        if (interface_str == vr::IVRServerDriverHost_Version) {
            rpc_wrapper = new RpcDriverHost(static_cast<vr::IVRServerDriverHost*>(real_interface));
        } else if (interface_str == vr::IVRDriverLog_Version) {
            rpc_wrapper = new RpcDriverLog(static_cast<vr::IVRDriverLog*>(real_interface));
        } else if (interface_str == vr::IVRSettings_Version) {
            rpc_wrapper = new RpcSettings(static_cast<vr::IVRSettings*>(real_interface));
        } else if (interface_str == "IVRDriverInput_003" || interface_str == vr::IVRDriverInput_Version) {
            rpc_wrapper = new RpcDriverInput(static_cast<vr::IVRDriverInput*>(real_interface));
        } else if (interface_str == vr::IVRDriverManager_Version) {
            rpc_wrapper = new RpcDriverManager(static_cast<vr::IVRDriverManager*>(real_interface));
        } else if (interface_str == vr::IVRProperties_Version) {
            rpc_wrapper = new RpcProperties(static_cast<vr::IVRProperties*>(real_interface));
        } else if (interface_str == vr::IVRResources_Version) {
            rpc_wrapper = new RpcResources(static_cast<vr::IVRResources*>(real_interface));
        } else {
            // If we don't have a specific wrapper, we can't vend it.
            if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;

            return nullptr;
        }

        if (rpc_wrapper) {
            interface_cache_[interface_str] = rpc_wrapper;
            return rpc_wrapper;
        }

        // If we don't have a wrapper, we can't return it over RPC.
        // For now, we'll return nullptr and an error.
        if (peError) *peError = vr::VRInitError_Init_InterfaceNotFound;
        return nullptr;
    }
}

vr::DriverHandle_t ClientContextManager::GetDriverHandle() {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetDriverHandle");
        return result.asUint64();
    } else {
        return real_context_->GetDriverHandle();
    }
}

bool hasStubbed = false;

// --- RpcServerTrackedDeviceProvider ---

RpcServerTrackedDeviceProvider::RpcServerTrackedDeviceProvider(vr::IServerTrackedDeviceProvider* real) : RpcObject(), real_provider_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "Init", [this](const auto& args){
            // The server receives the proxy to the client's context manager
            ClientContextManager* context_proxy = static_cast<ClientContextManager*>(args[0].asObject());
            
            auto result = this->Init(context_proxy);
            
            return RpcValue((int)result);
        });
        RpcSystem::RegisterFunction(prefix + "ShouldBlockStandbyMode", [this](const auto& args){
            return RpcValue((int)this->real_provider_->ShouldBlockStandbyMode());
        });
        
        RpcSystem::RegisterFunction(prefix + "Cleanup", [this](const auto& args){
            this->real_provider_->Cleanup();
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "RunFrame", [this](const auto& args){
            this->real_provider_->RunFrame();

            if (!hasStubbed) {
                // Stub out code at 0x11d0c0 with a ret in driver_playstation_vr2_orig.dll to avoid crash.
                HMODULE hModuleo = LoadLibraryW(L"C:\\Program Files (x86)\\Steam\\steamapps\\common\\PlayStation VR2 App\\SteamVR_Plug-In\\bin\\win64\\driver_playstation_vr2_orig.dll");

                void* function = reinterpret_cast<char*>(hModuleo) + 0x11d0c0;
                DWORD oldProtect;
                if (!VirtualProtect(function, 1, PAGE_EXECUTE_READWRITE, &oldProtect
                )) {
                    printf("VirtualProtect failed: %d\n", GetLastError());
                }
                unsigned char retInstruction = 0xC3; // x86 RET instruction
                SIZE_T bytesWritten;
                if (!WriteProcessMemory(GetCurrentProcess(), function, &retInstruction, 1,
                    &bytesWritten) || bytesWritten != 1) {
                    printf("WriteProcessMemory failed: %d\n", GetLastError());
                }
                if (!VirtualProtect(function, 1, oldProtect, &oldProtect)) {
                    printf("VirtualProtect restore failed: %d\n", GetLastError());
                }

                // Flush instruction cache to ensure modified code is used.
                if (!FlushInstructionCache(GetCurrentProcess(), function, 1)) {
                    printf("FlushInstructionCache failed: %d\n", GetLastError());
                }

                hasStubbed = true;
            }

            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "EnterStandby", [this](const auto& args){
            this->real_provider_->EnterStandby();
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "LeaveStandby", [this](const auto& args){
            this->real_provider_->LeaveStandby();
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "GetInterfaceVersions", [this](const auto& args){
            const char *const *versions = this->real_provider_->GetInterfaceVersions();
            if (!versions) {
                return RpcValue();
            }

            std::string concatenated_versions;
            for (int i = 0; versions[i] != nullptr; ++i) {
                concatenated_versions.append(versions[i]);
                concatenated_versions.push_back('\0'); // Use null char as separator
            }
            return RpcValue(concatenated_versions);
        });
    }
}

RpcServerTrackedDeviceProvider::RpcServerTrackedDeviceProvider(RpcObjectId id) : RpcObject(id), real_provider_(nullptr) {}

RpcServerTrackedDeviceProvider::~RpcServerTrackedDeviceProvider() {
    // The owner of the "real" provider is responsible for deleting it. This wrapper does not.
}

const std::string& RpcServerTrackedDeviceProvider::GetRpcClassName() const {
    static const std::string name = "IServerTrackedDeviceProvider";
    return name;
}

vr::EVRInitError RpcServerTrackedDeviceProvider::Init(vr::IVRDriverContext *pDriverContext) {
    if (IsProxy()) {
        // Client side. The provided context is from SteamVR and is real.
        VR_INIT_SERVER_DRIVER_CONTEXT(pDriverContext);
        
        // Create a manager for this context so the server can get interfaces from it.
        auto* context_manager = new ClientContextManager(pDriverContext);

        // Call the server's Init, passing a proxy to our context manager
        RpcValue result = RpcSystem::CallMethod(GetId(), "Init", RpcValue(context_manager));
        return (vr::EVRInitError)result.asInt();
    } else {
        return real_provider_->Init(pDriverContext);
    }
}

void RpcServerTrackedDeviceProvider::Cleanup() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "Cleanup");
    } else {
        real_provider_->Cleanup();
    }
}

const char *const *RpcServerTrackedDeviceProvider::GetInterfaceVersions() {
    if (IsProxy()) {
        // If we've already fetched them, return the cached version.
        if (!client_versions_ptrs_.empty()) {
            return client_versions_ptrs_.data();
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), "GetInterfaceVersions");
        if (result.isString()) {
            const std::string& concatenated = result.asString();
            size_t start = 0;
            while (start < concatenated.length()) {
                size_t end = concatenated.find('\0', start);
                if (end == std::string::npos) {
                    break; // Should not happen if server implementation is correct
                }
                client_versions_storage_.push_back(concatenated.substr(start, end - start));
                start = end + 1;
            }

            // Build the array of C-string pointers
            for (const auto& str : client_versions_storage_) {
                client_versions_ptrs_.push_back(str.c_str());
            }
            client_versions_ptrs_.push_back(nullptr); // Null-terminate the array
            return client_versions_ptrs_.data();
        }
        return nullptr;
    } else {
        return real_provider_->GetInterfaceVersions();
    }
}

void RpcServerTrackedDeviceProvider::RunFrame() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "RunFrame");
    } else {
        real_provider_->RunFrame();
    }
}

bool RpcServerTrackedDeviceProvider::ShouldBlockStandbyMode() {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), "ShouldBlockStandbyMode").asInt();
    } else {
        return real_provider_->ShouldBlockStandbyMode();
    }
}

void RpcServerTrackedDeviceProvider::EnterStandby() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "EnterStandby");
    } else {
        real_provider_->EnterStandby();
    }
}

void RpcServerTrackedDeviceProvider::LeaveStandby() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "LeaveStandby");
    } else {
        real_provider_->LeaveStandby();
    }
}

// --- RpcDriverHost ---

RpcDriverHost::RpcDriverHost(vr::IVRServerDriverHost* real) : RpcObject(), real_host_(real) {
    // The client-side object that wraps the real IVRServerDriverHost from SteamVR
    if (!IsProxy()) { // Client-side, real object
        // The server will call methods on its proxy, which will be forwarded here.
        // We need to register these methods so the RPC system can find them.
        std::string prefix = std::to_string(GetId()) + ".";

        RpcSystem::RegisterFunction(prefix + "TrackedDeviceAdded", [this](const auto& args) {
            // The temporary from asString() must be stored in a variable to extend its lifetime
            const std::string serial_str = args[0].asString();
            const char* serial = serial_str.c_str();
            auto device_class = (vr::ETrackedDeviceClass)args[1].asInt();
            auto* driver_proxy = static_cast<RpcTrackedDeviceServerDriver*>(args[2].asObject());

            bool result = this->real_host_->TrackedDeviceAdded(serial, device_class, driver_proxy);
            return RpcValue((int)result);
        });

        RpcSystem::RegisterFunction(prefix + "TrackedDevicePoseUpdated", [this](const auto& args){
            uint32_t which_device = (uint32_t)args[0].asInt();
            auto pose_data = args[1].asPointer();
            vr::DriverPose_t new_pose;
            if (pose_data.second == sizeof(vr::DriverPose_t)) {
                memcpy(&new_pose, pose_data.first, sizeof(vr::DriverPose_t));
                uint32_t pose_size = sizeof(vr::DriverPose_t);
                this->real_host_->TrackedDevicePoseUpdated(which_device, new_pose, pose_size);
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "VendorSpecificEvent", [this](const auto& args){
            uint32_t unWhichDevice = (uint32_t)args[0].asInt();
            auto eventType = (vr::EVREventType)args[1].asInt();
            auto eventDataPair = args[2].asPointer();
            double eventTimeOffset = (double)args[3].asFloat();

            vr::VREvent_Data_t eventData;
            if (eventDataPair.second == sizeof(vr::VREvent_Data_t))
            {
                memcpy(&eventData, eventDataPair.first, sizeof(vr::VREvent_Data_t));
                this->real_host_->VendorSpecificEvent(unWhichDevice, eventType, eventData, eventTimeOffset);
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "VsyncEvent", [this](const auto& args){
            this->real_host_->VsyncEvent(args[0].asFloat());
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "IsExiting", [this](const auto& args){
            return RpcValue((int)this->real_host_->IsExiting());
        });

        RpcSystem::RegisterFunction(prefix + "RequestRestart", [this](const auto& args){
            this->real_host_->RequestRestart(args[0].asString().c_str(), args[1].asString().c_str(), args[2].asString().c_str(), args[3].asString().c_str());
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "SetRecommendedRenderTargetSize", [this](const auto& args){
            this->real_host_->SetRecommendedRenderTargetSize((uint32_t)args[0].asInt(), (uint32_t)args[1].asInt(), (uint32_t)args[2].asInt());
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "PollNextEvent", [this](const auto& args){
            vr::VREvent_t event;

            int expected_size = args[0].asInt();

            if (this->real_host_->PollNextEvent(&event, sizeof(event))) {
                return RpcValue((const char*)&event, sizeof(event));
            }
            return RpcValue(); // Return null on no event
        });

        RpcSystem::RegisterFunction(prefix + "GetRawTrackedDevicePoses", [this](const auto& args){
            float fPredictedSecondsFromNow = args[0].asFloat();
            uint32_t unTrackedDevicePoseArrayCount = (uint32_t)args[1].asInt();
            if (unTrackedDevicePoseArrayCount > 0) {
                std::vector<vr::TrackedDevicePose_t> poses(unTrackedDevicePoseArrayCount);
                this->real_host_->GetRawTrackedDevicePoses(fPredictedSecondsFromNow, poses.data(), unTrackedDevicePoseArrayCount);
                return RpcValue((const char*)poses.data(), poses.size() * sizeof(vr::TrackedDevicePose_t));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "GetFrameTimings", [this](const auto& args){
            uint32_t nFrames = (uint32_t)args[0].asInt();
            if (nFrames > 0) {
                std::vector<vr::Compositor_FrameTiming> timings(nFrames);
                uint32_t framesRead = this->real_host_->GetFrameTimings(timings.data(), nFrames);
                // Return a pair: number of frames read, and the data buffer
                return RpcValue((const char*)timings.data(), framesRead * sizeof(vr::Compositor_FrameTiming));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "SetDisplayEyeToHead", [this](const auto& args){
            uint32_t unWhichDevice = (uint32_t)args[0].asInt();
            auto leftEyeData = args[1].asPointer();
            auto rightEyeData = args[2].asPointer();
            if (leftEyeData.second == sizeof(vr::HmdMatrix34_t) && rightEyeData.second == sizeof(vr::HmdMatrix34_t)) {
                this->real_host_->SetDisplayEyeToHead(unWhichDevice, *reinterpret_cast<const vr::HmdMatrix34_t*>(leftEyeData.first), *reinterpret_cast<const vr::HmdMatrix34_t*>(rightEyeData.first));
            }
            return RpcValue();
        });

        RpcSystem::RegisterFunction(prefix + "SetDisplayProjectionRaw", [this](const auto& args){
            uint32_t unWhichDevice = (uint32_t)args[0].asInt();
            auto leftEyeData = args[1].asPointer();
            auto rightEyeData = args[2].asPointer();
            if (leftEyeData.second == sizeof(vr::HmdRect2_t) && rightEyeData.second == sizeof(vr::HmdRect2_t)) {
                this->real_host_->SetDisplayProjectionRaw(unWhichDevice, *reinterpret_cast<const vr::HmdRect2_t*>(leftEyeData.first), *reinterpret_cast<const vr::HmdRect2_t*>(rightEyeData.first));
            }
            return RpcValue();
        });
    }
}

RpcDriverHost::RpcDriverHost(RpcObjectId id) : RpcObject(id) {}

RpcDriverHost::~RpcDriverHost() {}

const std::string& RpcDriverHost::GetRpcClassName() const {
    static const std::string name = "IVRServerDriverHost";
    return name;
}

bool RpcDriverHost::TrackedDeviceAdded(const char *pchDeviceSerialNumber, vr::ETrackedDeviceClass eDeviceClass, vr::ITrackedDeviceServerDriver *pDriver) {
    if (IsProxy()) {
        // This is called on the server (proxy) by the real driver.
        // We need to forward this to the client.
        // We wrap the server-side pDriver in an Rpc-enabled object.
        auto* rpc_driver = new RpcTrackedDeviceServerDriver(pDriver);

        // Now we can call the client's real TrackedDeviceAdded with our proxy object.
        RpcValue result = RpcSystem::CallMethod(GetId(), "TrackedDeviceAdded", RpcValue(std::string(pchDeviceSerialNumber)), RpcValue((int)eDeviceClass), RpcValue(rpc_driver));
        return result.asInt();
    }
    else
    {
        return real_host_ ? real_host_->TrackedDeviceAdded(pchDeviceSerialNumber, eDeviceClass, pDriver) : false;
    }
}

void RpcDriverHost::TrackedDevicePoseUpdated(uint32_t unWhichDevice, const vr::DriverPose_t &newPose, uint32_t unPoseStructSize) {
    if (IsProxy()) {
        // This is called on the server (proxy) by the real driver.
        // We need to forward this to the client.
        RpcSystem::CallMethod(GetId(), "TrackedDevicePoseUpdated", RpcValue((int)unWhichDevice), RpcValue((const char*)&newPose, unPoseStructSize));
    }
}

void RpcDriverHost::VsyncEvent(double vsyncTimeOffsetSeconds) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "VsyncEvent", RpcValue((float)vsyncTimeOffsetSeconds));
    }
}

void RpcDriverHost::VendorSpecificEvent(uint32_t unWhichDevice, vr::EVREventType eventType, const vr::VREvent_Data_t &eventData, double eventTimeOffset) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "VendorSpecificEvent",
            RpcValue((int)unWhichDevice),
            RpcValue((int)eventType),
            RpcValue((const char*)&eventData, sizeof(eventData)),
            RpcValue((float)eventTimeOffset)
        );
    }
}

bool RpcDriverHost::IsExiting() {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), "IsExiting").asInt();
    }
    return real_host_ ? real_host_->IsExiting() : true;
}

bool RpcDriverHost::PollNextEvent(vr::VREvent_t *pEvent, uint32_t uncbVREvent) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "PollNextEvent", RpcValue((int)uncbVREvent));
        if (result.isPointer()) {
            auto data = result.asPointer();
            if (data.second == sizeof(vr::VREvent_t)) {
                memcpy(pEvent, data.first, data.second);
                return true;
            }
            else {
                std::cerr << "Warning: PollNextEvent returned data of unexpected size " << data.second << std::endl;
            }
        }
        return false; // No event
    }
    return real_host_ ? real_host_->PollNextEvent(pEvent, uncbVREvent) : false;
}

void RpcDriverHost::GetRawTrackedDevicePoses(float fPredictedSecondsFromNow, vr::TrackedDevicePose_t *pTrackedDevicePoseArray, uint32_t unTrackedDevicePoseArrayCount) {
    if (IsProxy()) {
        if (!pTrackedDevicePoseArray || unTrackedDevicePoseArrayCount == 0) {
            return;
        }
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetRawTrackedDevicePoses", RpcValue(fPredictedSecondsFromNow), RpcValue((int)unTrackedDevicePoseArrayCount));
        if (result.isPointer()) {
            auto data = result.asPointer();
            size_t bytesToCopy = std::min(data.second, (size_t)unTrackedDevicePoseArrayCount * sizeof(vr::TrackedDevicePose_t));
            memcpy(pTrackedDevicePoseArray, data.first, bytesToCopy);
        }
    }
}

void RpcDriverHost::RequestRestart(const char *pchLocalizedReason, const char *pchExecutableToStart, const char *pchArguments, const char *pchWorkingDirectory) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "RequestRestart", RpcValue(std::string(pchLocalizedReason ? pchLocalizedReason : "")), RpcValue(std::string(pchExecutableToStart ? pchExecutableToStart : "")), RpcValue(std::string(pchArguments ? pchArguments : "")), RpcValue(std::string(pchWorkingDirectory ? pchWorkingDirectory : "")));
    }
}

uint32_t RpcDriverHost::GetFrameTimings(vr::Compositor_FrameTiming *pTiming, uint32_t nFrames) {
    if (IsProxy()) {
        if (!pTiming || nFrames == 0) {
            return 0;
        }
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetFrameTimings", RpcValue((int)nFrames));
        if (result.isPointer()) {
            auto data = result.asPointer();
            uint32_t framesRead = data.second / sizeof(vr::Compositor_FrameTiming);
            uint32_t framesToCopy = std::min(nFrames, framesRead);
            memcpy(pTiming, data.first, framesToCopy * sizeof(vr::Compositor_FrameTiming));
            return framesToCopy;
        }
        return 0;
    }
    return real_host_ ? real_host_->GetFrameTimings(pTiming, nFrames) : 0;
}

void RpcDriverHost::SetDisplayEyeToHead(uint32_t unWhichDevice, const vr::HmdMatrix34_t &eyeToHeadLeft, const vr::HmdMatrix34_t &eyeToHeadRight) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "SetDisplayEyeToHead",
            RpcValue((int)unWhichDevice),
            RpcValue((const char*)&eyeToHeadLeft, sizeof(eyeToHeadLeft)),
            RpcValue((const char*)&eyeToHeadRight, sizeof(eyeToHeadRight))
        );
    }
}

void RpcDriverHost::SetDisplayProjectionRaw(uint32_t unWhichDevice, const vr::HmdRect2_t &eyeLeft, const vr::HmdRect2_t &eyeRight) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "SetDisplayProjectionRaw",
            RpcValue((int)unWhichDevice),
            RpcValue((const char*)&eyeLeft, sizeof(eyeLeft)),
            RpcValue((const char*)&eyeRight, sizeof(eyeRight))
        );
    }
}

void RpcDriverHost::SetRecommendedRenderTargetSize(uint32_t unWhichDevice, uint32_t nWidth, uint32_t nHeight) {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "SetRecommendedRenderTargetSize", RpcValue((int)unWhichDevice), RpcValue((int)nWidth), RpcValue((int)nHeight));
    }
}


// --- RpcDriverLog ---

RpcDriverLog::RpcDriverLog(vr::IVRDriverLog* real) : RpcObject(), real_log_(real) {
    if (!IsProxy()) {
        RpcSystem::RegisterFunction(std::to_string(GetId()) + ".Log", [this](const auto& args){
            this->Log(args[0].asString().c_str());
            return RpcValue();
        });
    }
}

RpcDriverLog::RpcDriverLog(RpcObjectId id) : RpcObject(id) {}

RpcDriverLog::~RpcDriverLog() {}

const std::string& RpcDriverLog::GetRpcClassName() const {
    static const std::string name = "IVRDriverLog";
    return name;
}

void RpcDriverLog::Log(const char *pchLogMessage) {
    if (IsProxy()) {
        std::cout << "DriverLog: " << pchLogMessage << std::endl;
        RpcSystem::CallMethod(GetId(), "Log", RpcValue(std::string(pchLogMessage)));
    } else {
        real_log_->Log(pchLogMessage);
    }
}

// --- RpcSettings ---

RpcSettings::RpcSettings(vr::IVRSettings* real) : RpcObject(), real_settings_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "GetBool", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            bool result = this->real_settings_->GetBool(section.c_str(), key.c_str(), &err);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&result, (char*)&result + sizeof(result));
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "GetInt32", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            int32_t result = this->real_settings_->GetInt32(section.c_str(), key.c_str(), &err);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&result, (char*)&result + sizeof(result));
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "GetFloat", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            float result = this->real_settings_->GetFloat(section.c_str(), key.c_str(), &err);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&result, (char*)&result + sizeof(result));
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "GetString", [this](const auto& args) {
            char buffer[vr::k_unMaxPropertyStringSize];
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->real_settings_->GetString(section.c_str(), key.c_str(), buffer, sizeof(buffer), &err);
            
            std::vector<char> return_buffer;
            std::string result_str(buffer);
            return_buffer.insert(return_buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            return_buffer.insert(return_buffer.end(), result_str.begin(), result_str.end());
            return RpcValue(return_buffer.data(), return_buffer.size());
        });

        RpcSystem::RegisterFunction(prefix + "SetBool", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->real_settings_->SetBool(section.c_str(), key.c_str(), args[2].asInt(), &err);
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "SetInt32", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->real_settings_->SetInt32(section.c_str(), key.c_str(), args[2].asInt(), &err);
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "SetFloat", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->real_settings_->SetFloat(section.c_str(), key.c_str(), args[2].asFloat(), &err);
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "SetString", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            const std::string value = args[2].asString();
            this->real_settings_->SetString(section.c_str(), key.c_str(), value.c_str(), &err);
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "RemoveSection", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            this->real_settings_->RemoveSection(section.c_str(), &err);
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "RemoveKeyInSection", [this](const auto& args) {
            vr::EVRSettingsError err;
            const std::string section = args[0].asString();
            const std::string key = args[1].asString();
            this->real_settings_->RemoveKeyInSection(section.c_str(), key.c_str(), &err);
            return RpcValue((int)err);
        });
    }
}

RpcSettings::RpcSettings(RpcObjectId id) : RpcObject(id) {}

RpcSettings::~RpcSettings() {}

const std::string& RpcSettings::GetRpcClassName() const {
    static const std::string name = "IVRSettings";
    return name;
}

const char *RpcSettings::GetSettingsErrorNameFromEnum(vr::EVRSettingsError eError) {
    // This is a local call, no RPC needed.
    return real_settings_ ? real_settings_->GetSettingsErrorNameFromEnum(eError) : "VRSettingsError_None";
}

void RpcSettings::SetBool(const char *pchSection, const char *pchSettingsKey, bool bValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "SetBool", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue((int)bValue));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    } else {
        real_settings_->SetBool(pchSection, pchSettingsKey, bValue, peError);
    }
}

void RpcSettings::SetInt32(const char *pchSection, const char *pchSettingsKey, int32_t nValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "SetInt32", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue(nValue));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    } else {
        real_settings_->SetInt32(pchSection, pchSettingsKey, nValue, peError);
    }
}

void RpcSettings::SetFloat(const char *pchSection, const char *pchSettingsKey, float flValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "SetFloat", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue(flValue));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    } else {
        real_settings_->SetFloat(pchSection, pchSettingsKey, flValue, peError);
    }
}

void RpcSettings::SetString(const char *pchSection, const char *pchSettingsKey, const char *pchValue, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "SetString", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)), RpcValue(std::string(pchValue)));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    } else {
        real_settings_->SetString(pchSection, pchSettingsKey, pchValue, peError);
    }
}

bool RpcSettings::GetBool(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), "GetBool", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer() && result_val.asPointer().second == sizeof(bool) + sizeof(vr::EVRSettingsError)) {
            const char* ptr = result_val.asPointer().first;
            bool result = *reinterpret_cast<const bool*>(ptr);
            if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(ptr + sizeof(bool));
            return result;
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        return false;
    }
    return real_settings_ ? real_settings_->GetBool(pchSection, pchSettingsKey, peError) : false;
}

int32_t RpcSettings::GetInt32(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), "GetInt32", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer() && result_val.asPointer().second == sizeof(int32_t) + sizeof(vr::EVRSettingsError)) {
            const char* ptr = result_val.asPointer().first;
            int32_t result = *reinterpret_cast<const int32_t*>(ptr);
            if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(ptr + sizeof(int32_t));
            return result;
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        return 0;
    }
    return real_settings_ ? real_settings_->GetInt32(pchSection, pchSettingsKey, peError) : 0;
}

float RpcSettings::GetFloat(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), "GetFloat", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer() && result_val.asPointer().second == sizeof(float) + sizeof(vr::EVRSettingsError)) {
            const char* ptr = result_val.asPointer().first;
            float result = *reinterpret_cast<const float*>(ptr);
            if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(ptr + sizeof(float));
            return result;
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        return 0.0f;
    }
    return real_settings_ ? real_settings_->GetFloat(pchSection, pchSettingsKey, peError) : 0.0f;
}

void RpcSettings::GetString(const char *pchSection, const char *pchSettingsKey, VR_OUT_STRING() char *pchValue, uint32_t unValueLen, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result_val = RpcSystem::CallMethod(GetId(), "GetString", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (result_val.isPointer()) {
            auto data = result_val.asPointer();
            if (data.second >= sizeof(vr::EVRSettingsError)) {
                if (peError) *peError = *reinterpret_cast<const vr::EVRSettingsError*>(data.first);
                std::string result_str(data.first + sizeof(vr::EVRSettingsError), data.second - sizeof(vr::EVRSettingsError));
                strncpy_s(pchValue, unValueLen, result_str.c_str(), _TRUNCATE);
                return;
            }
        }
        if (peError) *peError = vr::VRSettingsError_ReadFailed;
        if (unValueLen > 0) pchValue[0] = '\0';
    } else if (real_settings_) {
        real_settings_->GetString(pchSection, pchSettingsKey, pchValue, unValueLen, peError);
    } else if (unValueLen > 0) {
        pchValue[0] = '\0';
    }
}

void RpcSettings::RemoveSection(const char *pchSection, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "RemoveSection", RpcValue(std::string(pchSection)));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    } else {
        real_settings_->RemoveSection(pchSection, peError);
    }
}

void RpcSettings::RemoveKeyInSection(const char *pchSection, const char *pchSettingsKey, vr::EVRSettingsError *peError) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "RemoveKeyInSection", RpcValue(std::string(pchSection)), RpcValue(std::string(pchSettingsKey)));
        if (peError) *peError = (vr::EVRSettingsError)result.asInt();
    } else {
        real_settings_->RemoveKeyInSection(pchSection, pchSettingsKey, peError);
    }
}

// --- RpcTrackedDeviceServerDriver ---

RpcTrackedDeviceServerDriver::RpcTrackedDeviceServerDriver(vr::ITrackedDeviceServerDriver* real) : RpcObject(), real_driver_(real) {
    if (!IsProxy()) { // Server-side, real object
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "Activate", [this](const auto& args){
            return RpcValue((int)this->Activate(args[0].asInt()));
        });
        RpcSystem::RegisterFunction(prefix + "Deactivate", [this](const auto& args){
            this->Deactivate();
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "EnterStandby", [this](const auto& args){
            this->EnterStandby();
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "DebugRequest", [this](const auto& args){
            char response_buf[vr::k_unMaxDriverDebugResponseSize];
            this->DebugRequest(args[0].asString().c_str(), response_buf, sizeof(response_buf));
            return RpcValue(std::string(response_buf));
        });

        RpcSystem::RegisterFunction(prefix + "GetPose", [this](const auto& args){
            vr::DriverPose_t pose = this->GetPose();
            return RpcValue((const char*)&pose, sizeof(pose));
        });
        RpcSystem::RegisterFunction(prefix + "GetComponent", [this](const auto& args){
            auto componentNameStr = args[0].asString();
            const char* componentName = componentNameStr.c_str();
            void* component = this->real_driver_->GetComponent(componentName);
            
            if (!component) {
                return RpcValue(); // Return null RpcValue
            }

            RpcObject* rpc_wrapper = nullptr;
            std::string interface_str = componentName;

            if (interface_str == vr::IVRDisplayComponent_Version) {
                rpc_wrapper = new RpcDisplayComponent(static_cast<vr::IVRDisplayComponent*>(component));
            } else if (interface_str == vr::IVRCameraComponent_Version) {
                rpc_wrapper = new RpcCameraComponent(static_cast<vr::IVRCameraComponent*>(component));
            } else {
                std::cerr << "Warning: GetComponent returned unknown interface " << interface_str << std::endl;
                return RpcValue(); // Unknown interface
            }

            return RpcValue(rpc_wrapper);
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
    if (IsProxy()) { // Client-side proxy
        return (vr::EVRInitError)RpcSystem::CallMethod(GetId(), "Activate", RpcValue((int)unObjectId)).asInt();
    } else { // Server-side real object
        return real_driver_->Activate(unObjectId);
    }
}

void RpcTrackedDeviceServerDriver::Deactivate() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "Deactivate");
    } else {
        real_driver_->Deactivate();
    }
}

void RpcTrackedDeviceServerDriver::EnterStandby() {
    if (IsProxy()) {
        RpcSystem::CallMethod(GetId(), "EnterStandby");
    } else {
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
            // Add other component types here as needed
        }
        return nullptr;
    } else { // Server-side real object
        void* component = real_driver_->GetComponent(pchComponentNameAndVersion);
        if (!component) {
            return nullptr;
        }

        RpcObject* rpc_wrapper = nullptr;
        std::string interface_str = pchComponentNameAndVersion;

        if (interface_str == vr::IVRDisplayComponent_Version) {
            rpc_wrapper = new RpcDisplayComponent(static_cast<vr::IVRDisplayComponent*>(component));
        } else if (interface_str == vr::IVRCameraComponent_Version) {
            rpc_wrapper = new RpcCameraComponent(static_cast<vr::IVRCameraComponent*>(component));
        }
        // Add other component types here as needed

        return rpc_wrapper;
    }
}

void RpcTrackedDeviceServerDriver::DebugRequest(const char *pchRequest, char *pchResponseBuffer, uint32_t unResponseBufferSize) {
    if (IsProxy()) {
        if (!pchResponseBuffer || unResponseBufferSize == 0) return;
        RpcValue result = RpcSystem::CallMethod(GetId(), "DebugRequest", RpcValue(std::string(pchRequest)));
        std::string response = result.asString();
        strncpy(pchResponseBuffer, response.c_str(), unResponseBufferSize);
        pchResponseBuffer[unResponseBufferSize - 1] = '\0';
    } else {
        real_driver_->DebugRequest(pchRequest, pchResponseBuffer, unResponseBufferSize);
    }
}

vr::DriverPose_t RpcTrackedDeviceServerDriver::GetPose() {
    if (IsProxy()) { // Client-side proxy
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetPose");
        if (result.isPointer() && result.asPointer().second == sizeof(vr::DriverPose_t)) {
            return *reinterpret_cast<const vr::DriverPose_t*>(result.asPointer().first);
        }
        return vr::DriverPose_t();
    } else { // Server-side real object
        return real_driver_->GetPose();
    }
}

// --- RpcDisplayComponent ---

RpcDisplayComponent::RpcDisplayComponent(vr::IVRDisplayComponent* real) : RpcObject(), real_component_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "GetWindowBounds", [this](const auto& args){
            int32_t x, y;
            uint32_t w, h;
            this->GetWindowBounds(&x, &y, &w, &h);
            int32_t data[] = {x, y, (int32_t)w, (int32_t)h};
            return RpcValue((const char*)data, sizeof(data));
        });
        RpcSystem::RegisterFunction(prefix + "IsDisplayOnDesktop", [this](const auto& args){
            return RpcValue((int)this->IsDisplayOnDesktop());
        });
        RpcSystem::RegisterFunction(prefix + "IsDisplayRealDisplay", [this](const auto& args){
            return RpcValue((int)this->IsDisplayRealDisplay());
        });
        RpcSystem::RegisterFunction(prefix + "GetRecommendedRenderTargetSize", [this](const auto& args){
            uint32_t w, h;
            this->GetRecommendedRenderTargetSize(&w, &h);
            uint32_t data[] = {w, h};
            return RpcValue((const char*)data, sizeof(data));
        });
        RpcSystem::RegisterFunction(prefix + "GetEyeOutputViewport", [this](const auto& args){
            uint32_t x, y, w, h;
            this->GetEyeOutputViewport((vr::EVREye)args[0].asInt(), &x, &y, &w, &h);
            uint32_t data[] = {x, y, w, h};
            return RpcValue((const char*)data, sizeof(data));
        });
        RpcSystem::RegisterFunction(prefix + "GetProjectionRaw", [this](const auto& args){
            float l, r, t, b;
            this->GetProjectionRaw((vr::EVREye)args[0].asInt(), &l, &r, &t, &b);
            float data[] = {l, r, t, b};
            return RpcValue((const char*)data, sizeof(data));
        });
        RpcSystem::RegisterFunction(prefix + "ComputeDistortion", [this](const auto& args){
            vr::DistortionCoordinates_t coords = this->ComputeDistortion((vr::EVREye)args[0].asInt(), args[1].asFloat(), args[2].asFloat());
            return RpcValue((const char*)&coords, sizeof(coords));
        });
        RpcSystem::RegisterFunction(prefix + "ComputeInverseDistortion", [this](const auto& args){
            vr::HmdVector2_t result;
            if (this->ComputeInverseDistortion(&result, (vr::EVREye)args[0].asInt(), (uint32_t)args[1].asInt(), args[2].asFloat(), args[3].asFloat())) {
                return RpcValue((const char*)&result, sizeof(result));
            }
            return RpcValue(); // Return null on failure
        });
    }
}
RpcDisplayComponent::RpcDisplayComponent(RpcObjectId id) : RpcObject(id) {}
RpcDisplayComponent::~RpcDisplayComponent() {}

const std::string& RpcDisplayComponent::GetRpcClassName() const {
    static const std::string name = "IVRDisplayComponent";
    return name;
}

void RpcDisplayComponent::GetWindowBounds(int32_t *pnX, int32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetWindowBounds");
        if (result.isPointer() && result.asPointer().second == sizeof(int32_t) * 4) {
            const int32_t* data = reinterpret_cast<const int32_t*>(result.asPointer().first);
            *pnX = data[0]; *pnY = data[1]; *pnWidth = data[2]; *pnHeight = data[3];
        }
    } else { real_component_->GetWindowBounds(pnX, pnY, pnWidth, pnHeight); }
}

bool RpcDisplayComponent::IsDisplayOnDesktop() {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "IsDisplayOnDesktop").asInt() : real_component_->IsDisplayOnDesktop();
}

bool RpcDisplayComponent::IsDisplayRealDisplay() {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "IsDisplayRealDisplay").asInt() : real_component_->IsDisplayRealDisplay();
}

void RpcDisplayComponent::GetRecommendedRenderTargetSize(uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetRecommendedRenderTargetSize");
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
            *pnWidth = data[0]; *pnHeight = data[1];
        }
    } else { real_component_->GetRecommendedRenderTargetSize(pnWidth, pnHeight); }
}

void RpcDisplayComponent::GetEyeOutputViewport(vr::EVREye eEye, uint32_t *pnX, uint32_t *pnY, uint32_t *pnWidth, uint32_t *pnHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetEyeOutputViewport", RpcValue((int)eEye));
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 4) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
            *pnX = data[0]; *pnY = data[1]; *pnWidth = data[2]; *pnHeight = data[3];
        }
    } else { real_component_->GetEyeOutputViewport(eEye, pnX, pnY, pnWidth, pnHeight); }
}

void RpcDisplayComponent::GetProjectionRaw(vr::EVREye eEye, float *pfLeft, float *pfRight, float *pfTop, float *pfBottom) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetProjectionRaw", RpcValue((int)eEye));
        if (result.isPointer() && result.asPointer().second == sizeof(float) * 4) {
            const float* data = reinterpret_cast<const float*>(result.asPointer().first);
            *pfLeft = data[0]; *pfRight = data[1]; *pfTop = data[2]; *pfBottom = data[3];
        }
    } else { real_component_->GetProjectionRaw(eEye, pfLeft, pfRight, pfTop, pfBottom); }
}

vr::DistortionCoordinates_t RpcDisplayComponent::ComputeDistortion(vr::EVREye eEye, float fU, float fV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "ComputeDistortion", RpcValue((int)eEye), RpcValue(fU), RpcValue(fV));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::DistortionCoordinates_t)) {
            return *reinterpret_cast<const vr::DistortionCoordinates_t*>(result.asPointer().first);
        }
        return vr::DistortionCoordinates_t();
    } else { return real_component_->ComputeDistortion(eEye, fU, fV); }
}

bool RpcDisplayComponent::ComputeInverseDistortion(vr::HmdVector2_t *pResult, vr::EVREye eEye, uint32_t unChannel, float fU, float fV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "ComputeInverseDistortion", RpcValue((int)eEye), RpcValue((int)unChannel), RpcValue(fU), RpcValue(fV));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::HmdVector2_t)) {
            if (pResult) {
                *pResult = *reinterpret_cast<const vr::HmdVector2_t*>(result.asPointer().first);
            }
            return true;
        }
        return false;
    } else {
        return real_component_->ComputeInverseDistortion(pResult, eEye, unChannel, fU, fV);
    }
}

// --- RpcCameraComponent ---

RpcCameraComponent::RpcCameraComponent(vr::IVRCameraComponent* real) : RpcObject(), real_component_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "GetCameraFrameDimensions", [this](const auto& args){
            uint32_t w, h;
            if (this->GetCameraFrameDimensions((vr::ECameraVideoStreamFormat)args[0].asInt(), &w, &h)) {
                uint32_t data[] = {w, h};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "GetCameraFrameBufferingRequirements", [this](const auto& args){
            int queue_size;
            uint32_t data_size;
            if (this->GetCameraFrameBufferingRequirements(&queue_size, &data_size)) {
                uint32_t data[] = {(uint32_t)queue_size, data_size};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "StartVideoStream", [this](const auto& args){
            return RpcValue((int)this->StartVideoStream());
        });
        RpcSystem::RegisterFunction(prefix + "StopVideoStream", [this](const auto& args){
            this->StopVideoStream();
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "IsVideoStreamActive", [this](const auto& args){
            bool paused;
            float elapsed;
            if (this->IsVideoStreamActive(&paused, &elapsed)) {
                float data[] = {(float)paused, elapsed};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "SetCameraVideoStreamFormat", [this](const auto& args){
            return RpcValue((int)this->SetCameraVideoStreamFormat((vr::ECameraVideoStreamFormat)args[0].asInt()));
        });
        RpcSystem::RegisterFunction(prefix + "GetCameraVideoStreamFormat", [this](const auto& args){
            return RpcValue((int)this->GetCameraVideoStreamFormat());
        });
        RpcSystem::RegisterFunction(prefix + "SetAutoExposure", [this](const auto& args){
            return RpcValue((int)this->SetAutoExposure(args[0].asInt()));
        });
        RpcSystem::RegisterFunction(prefix + "PauseVideoStream", [this](const auto& args){
            return RpcValue((int)this->PauseVideoStream());
        });
        RpcSystem::RegisterFunction(prefix + "ResumeVideoStream", [this](const auto& args){
            return RpcValue((int)this->ResumeVideoStream());
        });
        RpcSystem::RegisterFunction(prefix + "GetCameraDistortion", [this](const auto& args){
            float out_u, out_v;
            if (this->GetCameraDistortion((uint32_t)args[0].asInt(), args[1].asFloat(), args[2].asFloat(), &out_u, &out_v)) {
                float data[] = {out_u, out_v};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "GetCameraProjection", [this](const auto& args){
            vr::HmdMatrix44_t proj;
            if (this->GetCameraProjection((uint32_t)args[0].asInt(), (vr::EVRTrackedCameraFrameType)args[1].asInt(), args[2].asFloat(), args[3].asFloat(), &proj)) {
                return RpcValue((const char*)&proj, sizeof(proj));
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "SetFrameRate", [this](const auto& args){
            return RpcValue((int)this->SetFrameRate(args[0].asInt(), args[1].asInt()));
        });
        RpcSystem::RegisterFunction(prefix + "GetCameraCompatibilityMode", [this](const auto& args){
            vr::ECameraCompatibilityMode mode;
            if (this->GetCameraCompatibilityMode(&mode)) {
                return RpcValue((int)mode);
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "SetCameraCompatibilityMode", [this](const auto& args){
            return RpcValue((int)this->SetCameraCompatibilityMode((vr::ECameraCompatibilityMode)args[0].asInt()));
        });
        RpcSystem::RegisterFunction(prefix + "GetCameraFrameBounds", [this](const auto& args){
            uint32_t l, t, w, h;
            if (this->GetCameraFrameBounds((vr::EVRTrackedCameraFrameType)args[0].asInt(), &l, &t, &w, &h)) {
                uint32_t data[] = {l, t, w, h};
                return RpcValue((const char*)data, sizeof(data));
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "GetCameraIntrinsics", [this](const auto& args){
            vr::HmdVector2_t focal_length, center;
            vr::EVRDistortionFunctionType dist_type;
            double coeffs[vr::k_unMaxDistortionFunctionParameters];
            if (this->GetCameraIntrinsics((uint32_t)args[0].asInt(), (vr::EVRTrackedCameraFrameType)args[1].asInt(), &focal_length, &center, &dist_type, coeffs)) {
                std::vector<char> buffer;
                buffer.insert(buffer.end(), (char*)&focal_length, (char*)&focal_length + sizeof(focal_length));
                buffer.insert(buffer.end(), (char*)&center, (char*)&center + sizeof(center));
                buffer.insert(buffer.end(), (char*)&dist_type, (char*)&dist_type + sizeof(dist_type));
                buffer.insert(buffer.end(), (char*)coeffs, (char*)coeffs + sizeof(coeffs));
                return RpcValue(buffer.data(), buffer.size());
            }
            return RpcValue();
        });

        // Stubs for complex/unsupported methods
        RpcSystem::RegisterFunction(prefix + "SetCameraFrameBuffering", [](const auto& args){ return RpcValue(0); });
        RpcSystem::RegisterFunction(prefix + "GetVideoStreamFrame", [](const auto& args){ return RpcValue(); });
        RpcSystem::RegisterFunction(prefix + "ReleaseVideoStreamFrame", [](const auto& args){ return RpcValue(); });
        RpcSystem::RegisterFunction(prefix + "SetCameraVideoSinkCallback", [](const auto& args){ return RpcValue(0); });
    }
}
RpcCameraComponent::RpcCameraComponent(RpcObjectId id) : RpcObject(id) {}
RpcCameraComponent::~RpcCameraComponent() {}

const std::string& RpcCameraComponent::GetRpcClassName() const {
    static const std::string name = "IVRCameraComponent";
    return name;
}

bool RpcCameraComponent::GetCameraFrameDimensions(vr::ECameraVideoStreamFormat nVideoStreamFormat, uint32_t *pWidth, uint32_t *pHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraFrameDimensions", RpcValue((int)nVideoStreamFormat));
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
            *pWidth = data[0]; *pHeight = data[1];
            return true;
        }
        return false;
    } else { return real_component_->GetCameraFrameDimensions(nVideoStreamFormat, pWidth, pHeight); }
}

bool RpcCameraComponent::GetCameraFrameBufferingRequirements(int *pDefaultFrameQueueSize, uint32_t *pFrameBufferDataSize) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraFrameBufferingRequirements");
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 2) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
            *pDefaultFrameQueueSize = data[0]; *pFrameBufferDataSize = data[1];
            return true;
        }
        return false;
    } else { return real_component_->GetCameraFrameBufferingRequirements(pDefaultFrameQueueSize, pFrameBufferDataSize); }
}

bool RpcCameraComponent::StartVideoStream() {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "StartVideoStream").asInt() : real_component_->StartVideoStream();
}

void RpcCameraComponent::StopVideoStream() {
    if (IsProxy()) { RpcSystem::CallMethod(GetId(), "StopVideoStream"); } else { real_component_->StopVideoStream(); }
}

bool RpcCameraComponent::IsVideoStreamActive(bool *pbPaused, float *pflElapsedTime) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "IsVideoStreamActive");
        if (result.isPointer() && result.asPointer().second == sizeof(float) * 2) {
            const float* data = reinterpret_cast<const float*>(result.asPointer().first);
            *pbPaused = (bool)data[0]; *pflElapsedTime = data[1];
            return true;
        }
        return false;
    } else { return real_component_->IsVideoStreamActive(pbPaused, pflElapsedTime); }
}

bool RpcCameraComponent::SetCameraFrameBuffering(int nFrameBufferCount, void **ppFrameBuffers, uint32_t nFrameBufferDataSize) {
    // Stub: This is too complex to marshal over RPC. Frame buffers are memory regions that would need to be shared.
    // Also may not be used at all.
    return false;
}

bool RpcCameraComponent::SetCameraVideoStreamFormat(vr::ECameraVideoStreamFormat nVideoStreamFormat) {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "SetCameraVideoStreamFormat", RpcValue((int)nVideoStreamFormat)).asInt() : real_component_->SetCameraVideoStreamFormat(nVideoStreamFormat);
}

vr::ECameraVideoStreamFormat RpcCameraComponent::GetCameraVideoStreamFormat() {
    return IsProxy() ? (vr::ECameraVideoStreamFormat)RpcSystem::CallMethod(GetId(), "GetCameraVideoStreamFormat").asInt() : real_component_->GetCameraVideoStreamFormat();
}

const vr::CameraVideoStreamFrame_t *RpcCameraComponent::GetVideoStreamFrame() {
    // Stub: Returning a pointer to a complex struct that contains image data is not feasible with this RPC system.
    // A more advanced implementation would involve serializing the frame data and managing its lifecycle across processes.
    // Also may not be used at all.
    return nullptr;
}

void RpcCameraComponent::ReleaseVideoStreamFrame(const vr::CameraVideoStreamFrame_t *pFrameImage) {
    // Stub: Companion to GetVideoStreamFrame.
    // Also may not be used at all.
}

bool RpcCameraComponent::SetAutoExposure(bool bEnable) {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "SetAutoExposure", RpcValue((int)bEnable)).asInt() : real_component_->SetAutoExposure(bEnable);
}

bool RpcCameraComponent::PauseVideoStream() {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "PauseVideoStream").asInt() : real_component_->PauseVideoStream();
}

bool RpcCameraComponent::ResumeVideoStream() {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "ResumeVideoStream").asInt() : real_component_->ResumeVideoStream();
}

bool RpcCameraComponent::GetCameraDistortion(uint32_t nCameraIndex, float flInputU, float flInputV, float *pflOutputU, float *pflOutputV) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraDistortion", RpcValue((int)nCameraIndex), RpcValue(flInputU), RpcValue(flInputV));
        if (result.isPointer() && result.asPointer().second == sizeof(float) * 2) {
            const float* data = reinterpret_cast<const float*>(result.asPointer().first);
            *pflOutputU = data[0]; *pflOutputV = data[1];
            return true;
        }
        return false;
    } else {
        return real_component_->GetCameraDistortion(nCameraIndex, flInputU, flInputV, pflOutputU, pflOutputV);
    }
}

bool RpcCameraComponent::GetCameraProjection(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, float flZNear, float flZFar, vr::HmdMatrix44_t *pProjection) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraProjection", RpcValue((int)nCameraIndex), RpcValue((int)eFrameType), RpcValue(flZNear), RpcValue(flZFar));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::HmdMatrix44_t)) {
            *pProjection = *reinterpret_cast<const vr::HmdMatrix44_t*>(result.asPointer().first);
            return true;
        }
        return false;
    } else {
        return real_component_->GetCameraProjection(nCameraIndex, eFrameType, flZNear, flZFar, pProjection);
    }
}

bool RpcCameraComponent::SetFrameRate(int nISPFrameRate, int nSensorFrameRate) {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "SetFrameRate", RpcValue(nISPFrameRate), RpcValue(nSensorFrameRate)).asInt() : real_component_->SetFrameRate(nISPFrameRate, nSensorFrameRate);
}

bool RpcCameraComponent::SetCameraVideoSinkCallback(vr::ICameraVideoSinkCallback *pCameraVideoSinkCallback) {
    // Stub: This may not be used at all.
    return false;
}

bool RpcCameraComponent::GetCameraCompatibilityMode(vr::ECameraCompatibilityMode *pCameraCompatibilityMode) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraCompatibilityMode");
        if (result.isInt()) {
            *pCameraCompatibilityMode = (vr::ECameraCompatibilityMode)result.asInt();
            return true;
        }
        return false;
    } else {
        return real_component_->GetCameraCompatibilityMode(pCameraCompatibilityMode);
    }
}

bool RpcCameraComponent::SetCameraCompatibilityMode(vr::ECameraCompatibilityMode nCameraCompatibilityMode) {
    return IsProxy() ? RpcSystem::CallMethod(GetId(), "SetCameraCompatibilityMode", RpcValue((int)nCameraCompatibilityMode)).asInt() : real_component_->SetCameraCompatibilityMode(nCameraCompatibilityMode);
}

bool RpcCameraComponent::GetCameraFrameBounds(vr::EVRTrackedCameraFrameType eFrameType, uint32_t *pLeft, uint32_t *pTop, uint32_t *pWidth, uint32_t *pHeight) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraFrameBounds", RpcValue((int)eFrameType));
        if (result.isPointer() && result.asPointer().second == sizeof(uint32_t) * 4) {
            const uint32_t* data = reinterpret_cast<const uint32_t*>(result.asPointer().first);
            *pLeft = data[0]; *pTop = data[1]; *pWidth = data[2]; *pHeight = data[3];
            return true;
        }
        return false;
    } else {
        return real_component_->GetCameraFrameBounds(eFrameType, pLeft, pTop, pWidth, pHeight);
    }
}

bool RpcCameraComponent::GetCameraIntrinsics(uint32_t nCameraIndex, vr::EVRTrackedCameraFrameType eFrameType, vr::HmdVector2_t *pFocalLength, vr::HmdVector2_t *pCenter, vr::EVRDistortionFunctionType *peDistortionType, double rCoefficients[vr::k_unMaxDistortionFunctionParameters]) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetCameraIntrinsics", RpcValue((int)nCameraIndex), RpcValue((int)eFrameType));
        if (result.isPointer()) {
            auto data = result.asPointer();
            const char* ptr = data.first;
            size_t expected_size = sizeof(vr::HmdVector2_t) * 2 + sizeof(vr::EVRDistortionFunctionType) + sizeof(double) * vr::k_unMaxDistortionFunctionParameters;
            if (data.second == expected_size) {
                memcpy(pFocalLength, ptr, sizeof(vr::HmdVector2_t));
                ptr += sizeof(vr::HmdVector2_t);
                memcpy(pCenter, ptr, sizeof(vr::HmdVector2_t));
                ptr += sizeof(vr::HmdVector2_t);
                memcpy(peDistortionType, ptr, sizeof(vr::EVRDistortionFunctionType));
                ptr += sizeof(vr::EVRDistortionFunctionType);
                memcpy(rCoefficients, ptr, sizeof(double) * vr::k_unMaxDistortionFunctionParameters);
                return true;
            }
        }
        return false;
    } else {
        return real_component_->GetCameraIntrinsics(nCameraIndex, eFrameType, pFocalLength, pCenter, peDistortionType, rCoefficients);
    }
}

// --- RpcDriverInput ---

RpcDriverInput::RpcDriverInput(vr::IVRDriverInput* real) : RpcObject(), real_input_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";

        RpcSystem::RegisterFunction(prefix + "CreateBooleanComponent", [this](const auto& args){
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->real_input_->CreateBooleanComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "UpdateBooleanComponent", [this](const auto& args){
            vr::EVRInputError err = this->real_input_->UpdateBooleanComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), args[1].asInt(), args[2].asFloat());
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "CreateScalarComponent", [this](const auto& args){
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->real_input_->CreateScalarComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle, (vr::EVRScalarType)args[2].asInt(), (vr::EVRScalarUnits)args[3].asInt());
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "UpdateScalarComponent", [this](const auto& args){
            vr::EVRInputError err = this->real_input_->UpdateScalarComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), args[1].asFloat(), args[2].asFloat());
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "CreateHapticComponent", [this](const auto& args){
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->real_input_->CreateHapticComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "CreateSkeletonComponent", [this](const auto& args){
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string name_str = args[1].asString();
            const std::string skeleton_path_str = args[2].asString();
            const std::string base_pose_path_str = args[3].asString();
            auto grip_transforms_data = args[5].asPointer();
            vr::EVRInputError err = this->real_input_->CreateSkeletonComponent(
                (vr::PropertyContainerHandle_t)args[0].asUint64(),
                name_str.c_str(),
                skeleton_path_str.c_str(),
                base_pose_path_str.c_str(),
                (vr::EVRSkeletalTrackingLevel)args[4].asInt(),
                (const vr::VRBoneTransform_t *)grip_transforms_data.first,
                (uint32_t)args[6].asInt(), &handle );
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "UpdateSkeletonComponent", [this](const auto& args){
            auto transforms_data = args[2].asPointer();
            vr::EVRInputError err = this->real_input_->UpdateSkeletonComponent( (vr::VRInputComponentHandle_t)args[0].asUint64(), (vr::EVRSkeletalMotionRange)args[1].asInt(), (const vr::VRBoneTransform_t *)transforms_data.first, (uint32_t)args[3].asInt() );
            return RpcValue((int)err);
        });
        RpcSystem::RegisterFunction(prefix + "CreatePoseComponent", [this](const auto& args){
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->real_input_->CreatePoseComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "UpdatePoseComponent", [this](const auto& args){
            vr::EVRInputError err = this->real_input_->UpdatePoseComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), (const vr::HmdMatrix34_t*)args[1].asPointer().first, args[2].asFloat());
            return RpcValue((int)err);
        });
         RpcSystem::RegisterFunction(prefix + "CreateEyeTrackingComponent", [this](const auto& args){
            vr::VRInputComponentHandle_t handle = vr::k_ulInvalidInputComponentHandle;
            const std::string pchName_str = args[1].asString();
            vr::EVRInputError err = this->real_input_->CreateEyeTrackingComponent((vr::PropertyContainerHandle_t)args[0].asUint64(), pchName_str.c_str(), &handle);
            std::vector<char> buffer;
            buffer.insert(buffer.end(), (char*)&err, (char*)&err + sizeof(err));
            buffer.insert(buffer.end(), (char*)&handle, (char*)&handle + sizeof(handle));
            return RpcValue(buffer.data(), buffer.size());
        });
        RpcSystem::RegisterFunction(prefix + "UpdateEyeTrackingComponent", [this](const auto& args){
            const auto& arg = args[1].asPointer();
            if(arg.second != sizeof(vr::VREyeTrackingData_t)) {
                return RpcValue((int)vr::VRInputError_InvalidParam);
            }
            vr::EVRInputError err = this->real_input_->UpdateEyeTrackingComponent((vr::VRInputComponentHandle_t)args[0].asUint64(), (const vr::VREyeTrackingData_t*)arg.first, args[2].asFloat());
            return RpcValue((int)err);
        });
    }
}
RpcDriverInput::RpcDriverInput(RpcObjectId id) : RpcObject(id) {}
RpcDriverInput::~RpcDriverInput() {}

const std::string& RpcDriverInput::GetRpcClassName() const {
    static const std::string name = "IVRDriverInput";
    return name;
}

vr::EVRInputError RpcDriverInput::CreateBooleanComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
     if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "CreateBooleanComponent", RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
    } else {
        return real_input_->CreateBooleanComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdateBooleanComponent(vr::VRInputComponentHandle_t ulComponent, bool bNewValue, double fTimeOffset) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), "UpdateBooleanComponent", RpcValue(ulComponent), RpcValue((int)bNewValue), RpcValue((float)fTimeOffset)).asInt();
    } else {
        return real_input_->UpdateBooleanComponent(ulComponent, bNewValue, fTimeOffset);
    }
}

vr::EVRInputError RpcDriverInput::CreateScalarComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle, vr::EVRScalarType eType, vr::EVRScalarUnits eUnits) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "CreateScalarComponent", RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)), RpcValue((int)eType), RpcValue((int)eUnits));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
     } else {
        return real_input_->CreateScalarComponent(ulContainer, pchName, pHandle, eType, eUnits);
    }
}

vr::EVRInputError RpcDriverInput::UpdateScalarComponent(vr::VRInputComponentHandle_t ulComponent, float fNewValue, double fTimeOffset) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), "UpdateScalarComponent", RpcValue(ulComponent), RpcValue(fNewValue), RpcValue((float)fTimeOffset)).asInt();
    } else {
        return real_input_->UpdateScalarComponent(ulComponent, fNewValue, fTimeOffset);
    }
}

vr::EVRInputError RpcDriverInput::CreateHapticComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "CreateHapticComponent", RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
     } else {
        return real_input_->CreateHapticComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::CreateSkeletonComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, const char *pchSkeletonPath, const char *pchBasePosePath, vr::EVRSkeletalTrackingLevel eSkeletalTrackingLevel, const vr::VRBoneTransform_t *pGripLimitTransforms, uint32_t unGripLimitTransformCount, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "CreateSkeletonComponent",
            RpcValue((uint64_t)ulContainer),
            RpcValue(std::string(pchName)),
            RpcValue(std::string(pchSkeletonPath)),
            RpcValue(std::string(pchBasePosePath)),
            RpcValue((int)eSkeletalTrackingLevel),
            RpcValue((const char*)pGripLimitTransforms, unGripLimitTransformCount * sizeof(vr::VRBoneTransform_t)),
            RpcValue((int)unGripLimitTransformCount)
        );
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
    } else {
        return real_input_->CreateSkeletonComponent(ulContainer, pchName, pchSkeletonPath, pchBasePosePath, eSkeletalTrackingLevel, pGripLimitTransforms, unGripLimitTransformCount, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdateSkeletonComponent(vr::VRInputComponentHandle_t ulComponent, vr::EVRSkeletalMotionRange eMotionRange, const vr::VRBoneTransform_t *pTransforms, uint32_t unTransformCount) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), "UpdateSkeletonComponent",
            RpcValue(ulComponent),
            RpcValue((int)eMotionRange),
            RpcValue((const char*)pTransforms, unTransformCount * sizeof(vr::VRBoneTransform_t)),
            RpcValue((int)unTransformCount)
        ).asInt();
    } else {
        return real_input_->UpdateSkeletonComponent(ulComponent, eMotionRange, pTransforms, unTransformCount);
    }
}

vr::EVRInputError RpcDriverInput::CreatePoseComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "CreatePoseComponent", RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
        if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
        }
        *pHandle = vr::k_ulInvalidInputComponentHandle;
        return vr::VRInputError_IPCError;
     } else {
        return real_input_->CreatePoseComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdatePoseComponent(vr::VRInputComponentHandle_t ulComponent, const vr::HmdMatrix34_t *pMatPoseOffset, double fTimeOffset) {
     if (IsProxy()) {
         return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), "UpdatePoseComponent", RpcValue(ulComponent), RpcValue((const char*)pMatPoseOffset, sizeof(vr::HmdMatrix34_t)), RpcValue((float)fTimeOffset)).asInt();
     } else {
         return real_input_->UpdatePoseComponent(ulComponent, pMatPoseOffset, fTimeOffset);
     }
}

vr::EVRInputError RpcDriverInput::CreateEyeTrackingComponent(vr::PropertyContainerHandle_t ulContainer, const char *pchName, vr::VRInputComponentHandle_t *pHandle) {
     if (IsProxy()) {
         RpcValue result = RpcSystem::CallMethod(GetId(), "CreateEyeTrackingComponent", RpcValue((uint64_t)ulContainer), RpcValue(std::string(pchName)));
         if (result.isPointer() && result.asPointer().second == sizeof(vr::EVRInputError) + sizeof(vr::VRInputComponentHandle_t)) {
            const char* ptr = result.asPointer().first;
            vr::EVRInputError err = *reinterpret_cast<const vr::EVRInputError*>(ptr);
            *pHandle = *reinterpret_cast<const vr::VRInputComponentHandle_t*>(ptr + sizeof(vr::EVRInputError));
            return err;
         }
         *pHandle = vr::k_ulInvalidInputComponentHandle;
         return vr::VRInputError_IPCError;
     } else {
        return real_input_->CreateEyeTrackingComponent(ulContainer, pchName, pHandle);
    }
}

vr::EVRInputError RpcDriverInput::UpdateEyeTrackingComponent(vr::VRInputComponentHandle_t ulComponent, const vr::VREyeTrackingData_t *pEyeTrackingData, double fTimeOffset) {
    if (IsProxy()) {
        return (vr::EVRInputError)RpcSystem::CallMethod(GetId(), "UpdateEyeTrackingComponent", RpcValue(ulComponent), RpcValue((const char*)pEyeTrackingData, sizeof(vr::VREyeTrackingData_t)), RpcValue((float)fTimeOffset)).asInt();
    } else {
        return real_input_->UpdateEyeTrackingComponent(ulComponent, pEyeTrackingData, fTimeOffset);
    }
}

// --- RpcDriverManager ---

RpcDriverManager::RpcDriverManager(vr::IVRDriverManager* real) : RpcObject(), real_manager_(real) {
    if (!IsProxy()) { // Real object on client
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "GetDriverCount", [this](const auto& args){
            return RpcValue((int)this->GetDriverCount());
        });
        RpcSystem::RegisterFunction(prefix + "GetDriverName", [this](const auto& args){
            char buffer[1024];
            uint32_t result = this->GetDriverName((vr::DriverId_t)args[0].asInt(), buffer, sizeof(buffer));
            return RpcValue(std::string(buffer, result));
        });
        RpcSystem::RegisterFunction(prefix + "GetDriverHandle", [this](const auto& args){
            return RpcValue((int)this->GetDriverHandle(args[0].asString().c_str()));
        });
        RpcSystem::RegisterFunction(prefix + "IsEnabled", [this](const auto& args){
            return RpcValue((int)this->IsEnabled((vr::DriverId_t)args[0].asInt()));
        });
    }
}
RpcDriverManager::RpcDriverManager(RpcObjectId id) : RpcObject(id) {}
RpcDriverManager::~RpcDriverManager() {}

const std::string& RpcDriverManager::GetRpcClassName() const {
    static const std::string name = "IVRDriverManager";
    return name;
}

uint32_t RpcDriverManager::GetDriverCount() const {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), "GetDriverCount").asInt();
    } else {
        return real_manager_->GetDriverCount();
    }
}

uint32_t RpcDriverManager::GetDriverName(vr::DriverId_t nDriver, char *pchValue, uint32_t unBufferSize) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetDriverName", RpcValue((int)nDriver));
        std::string name = result.asString();
        if (pchValue && unBufferSize > 0) {
            strncpy(pchValue, name.c_str(), unBufferSize);
            pchValue[unBufferSize - 1] = '\0';
        }
        return name.length() + 1;
    } else {
        return real_manager_->GetDriverName(nDriver, pchValue, unBufferSize);
    }
}

vr::DriverHandle_t RpcDriverManager::GetDriverHandle(const char *pchDriverName) {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), "GetDriverHandle", RpcValue(std::string(pchDriverName))).asInt();
    } else {
        return real_manager_->GetDriverHandle(pchDriverName);
    }
}

bool RpcDriverManager::IsEnabled(vr::DriverId_t nDriver) const {
    if (IsProxy()) {
        return RpcSystem::CallMethod(GetId(), "IsEnabled", RpcValue((int)nDriver)).asInt();
    } else {
        return real_manager_->IsEnabled(nDriver);
    }
}

// --- RpcProperties ---

RpcProperties::RpcProperties(vr::IVRProperties* real) : RpcObject(), real_properties_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "GetPropErrorNameFromEnum", [this](const auto& args) {
            const char* result_cstr = this->GetPropErrorNameFromEnum((vr::ETrackedPropertyError)args[0].asInt());
            std::string result = result_cstr ? result_cstr : "";
            return RpcValue(result);
        });
        RpcSystem::RegisterFunction(prefix + "TrackedDeviceToPropertyContainer", [this](const auto& args){
            vr::PropertyContainerHandle_t handle = this->TrackedDeviceToPropertyContainer((vr::TrackedDeviceIndex_t)args[0].asInt());
            return RpcValue(static_cast<uint64_t>(handle));
        });

        RpcSystem::RegisterFunction(prefix + "WritePropertyBatch", [this](const auto& args) {
            vr::PropertyContainerHandle_t ulContainerHandle = args[0].asUint64();
            auto batch_data = args[1].asPointer();
            const char* ptr = batch_data.first;

            uint32_t unBatchEntryCount;
            memcpy(&unBatchEntryCount, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            std::vector<vr::PropertyWrite_t> batch(unBatchEntryCount);
            std::vector<std::vector<char>> data_buffers(unBatchEntryCount);

            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                memcpy(&batch[i].prop, ptr, sizeof(vr::ETrackedDeviceProperty));
                ptr += sizeof(vr::ETrackedDeviceProperty);
                memcpy(&batch[i].writeType, ptr, sizeof(vr::EPropertyWriteType));
                ptr += sizeof(vr::EPropertyWriteType);
                memcpy(&batch[i].unTag, ptr, sizeof(vr::PropertyTypeTag_t));
                ptr += sizeof(vr::PropertyTypeTag_t);
                memcpy(&batch[i].unBufferSize, ptr, sizeof(uint32_t));
                ptr += sizeof(uint32_t);

                if (batch[i].unBufferSize > 0) {
                    data_buffers[i].assign(ptr, ptr + batch[i].unBufferSize);
                    batch[i].pvBuffer = data_buffers[i].data();
                    ptr += batch[i].unBufferSize;
                } else {
                    batch[i].pvBuffer = nullptr;
                }
            }

            vr::ETrackedPropertyError overallError = this->real_properties_->WritePropertyBatch(ulContainerHandle, batch.data(), unBatchEntryCount);

            std::vector<char> return_buffer;
            return_buffer.insert(return_buffer.end(), (char*)&overallError, (char*)&overallError + sizeof(overallError));
            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].eError, (char*)&batch[i].eError + sizeof(vr::ETrackedPropertyError));
            }
            return RpcValue(return_buffer.data(), return_buffer.size());
        });

        RpcSystem::RegisterFunction(prefix + "ReadPropertyBatch", [this](const auto& args) {
            vr::PropertyContainerHandle_t ulContainerHandle = args[0].asUint64();
            auto batch_data = args[1].asPointer();
            const char* ptr = batch_data.first;

            uint32_t unBatchEntryCount;
            memcpy(&unBatchEntryCount, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            std::vector<vr::PropertyRead_t> batch(unBatchEntryCount);
            std::vector<std::vector<char>> data_buffers(unBatchEntryCount);

            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                memcpy(&batch[i].prop, ptr, sizeof(vr::ETrackedDeviceProperty));
                ptr += sizeof(vr::ETrackedDeviceProperty);
                memcpy(&batch[i].unBufferSize, ptr, sizeof(uint32_t));
                ptr += sizeof(uint32_t);
                data_buffers[i].resize(batch[i].unBufferSize);
                batch[i].pvBuffer = data_buffers[i].data();
            }

            vr::ETrackedPropertyError overallError = this->real_properties_->ReadPropertyBatch(ulContainerHandle, batch.data(), unBatchEntryCount);

            std::vector<char> return_buffer;
            return_buffer.insert(return_buffer.end(), (char*)&overallError, (char*)&overallError + sizeof(overallError));
            // The pvBuffer in batch[i] is a local pointer on the server and should not be sent.
            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                vr::PropertyRead_t entry_to_send = batch[i];
                entry_to_send.pvBuffer = nullptr; // This pointer is not valid on the client
                return_buffer.insert(return_buffer.end(), (char*)&entry_to_send, (char*)&entry_to_send + sizeof(vr::PropertyRead_t));
                if (batch[i].eError == vr::TrackedProp_Success && batch[i].unRequiredBufferSize > 0) {
                    return_buffer.insert(return_buffer.end(), (char*)batch[i].pvBuffer, (char*)batch[i].pvBuffer + batch[i].unRequiredBufferSize);
                }
            }
            return RpcValue(return_buffer.data(), return_buffer.size());
        });
    }
}
RpcProperties::RpcProperties(RpcObjectId id) : RpcObject(id) {}
RpcProperties::~RpcProperties() {}

const std::string& RpcProperties::GetRpcClassName() const {
    static const std::string name = "IVRProperties";
    return name;
}

vr::ETrackedPropertyError RpcProperties::ReadPropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyRead_t *pBatch, uint32_t unBatchEntryCount) {
    if (IsProxy()) {
        std::vector<char> request_buffer;
        request_buffer.insert(request_buffer.end(), (char*)&unBatchEntryCount, (char*)&unBatchEntryCount + sizeof(uint32_t));
        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].prop, (char*)&pBatch[i].prop + sizeof(vr::ETrackedDeviceProperty));
            request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].unBufferSize, (char*)&pBatch[i].unBufferSize + sizeof(uint32_t));

            // Log property being requested
            std::cout << "Requesting property: " << pBatch[i].prop << " with buffer size: " << pBatch[i].unBufferSize << std::endl;
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), "ReadPropertyBatch", RpcValue(ulContainerHandle), RpcValue(request_buffer.data(), request_buffer.size()));

        if (!result.isPointer()) {
            return vr::TrackedProp_IPCReadFailure;
        }

        auto response_data = result.asPointer();
        const char* ptr = response_data.first;
        const char* end_ptr = ptr + response_data.second;

        PropertyReadBatchResult batchResult;
        batchResult.batch.resize(unBatchEntryCount);

        memcpy(&batchResult.overallError, ptr, sizeof(vr::ETrackedPropertyError));
        ptr += sizeof(vr::ETrackedPropertyError);

        // First, deserialize all the PropertyRead_t structs and calculate total buffer size
        size_t total_data_size = 0;
        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            if (ptr + sizeof(vr::PropertyRead_t) > end_ptr) return vr::TrackedProp_IPCReadFailure;
            memcpy(&batchResult.batch[i], ptr, sizeof(vr::PropertyRead_t));
            ptr += sizeof(vr::PropertyRead_t);
            if (batchResult.batch[i].eError == vr::TrackedProp_Success && batchResult.batch[i].unRequiredBufferSize > 0) {
                total_data_size += batchResult.batch[i].unRequiredBufferSize;
            }
        }

        // Allocate a single buffer for all property data
        batchResult.data_buffer.assign(ptr, ptr + total_data_size);
        if (batchResult.data_buffer.size() != total_data_size) return vr::TrackedProp_IPCReadFailure;

        // Now, copy results to the user's buffer and patch pointers
        char* current_data_ptr = batchResult.data_buffer.data();

        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            pBatch[i].prop = batchResult.batch[i].prop;
            pBatch[i].unBufferSize = batchResult.batch[i].unBufferSize;
            pBatch[i].unTag = batchResult.batch[i].unTag;
            pBatch[i].unRequiredBufferSize = batchResult.batch[i].unRequiredBufferSize;
            pBatch[i].eError = batchResult.batch[i].eError;


            if (pBatch[i].eError == vr::TrackedProp_Success && pBatch[i].unRequiredBufferSize > 0) {
                uint32_t bytesToCopy = std::min(pBatch[i].unBufferSize, pBatch[i].unRequiredBufferSize);
                if (pBatch[i].pvBuffer && bytesToCopy > 0) {
                    memcpy(pBatch[i].pvBuffer, current_data_ptr, bytesToCopy);
                }
                current_data_ptr += pBatch[i].unRequiredBufferSize;
            }

            std::cout << "Received property: " << pBatch[i].prop << " with error: " << pBatch[i].eError << "tag type: " << pBatch[i].unTag << " and required buffer size: " << pBatch[i].unRequiredBufferSize << std::endl;
            if (pBatch[i].eError == vr::TrackedProp_Success && pBatch[i].unRequiredBufferSize > 0 && pBatch[i].pvBuffer) {
                std::string value_str((char*)pBatch[i].pvBuffer, pBatch[i].unRequiredBufferSize);
                std::cout << "Property value: " << value_str << std::endl;
            }
        }
        return batchResult.overallError;
    }
    return real_properties_ ? real_properties_->ReadPropertyBatch(ulContainerHandle, pBatch, unBatchEntryCount) : vr::TrackedProp_InvalidOperation;
}
vr::ETrackedPropertyError RpcProperties::WritePropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyWrite_t *pBatch, uint32_t unBatchEntryCount) {
    if (IsProxy()) {
        std::vector<char> buffer;
        buffer.insert(buffer.end(), (char*)&unBatchEntryCount, (char*)&unBatchEntryCount + sizeof(uint32_t));
        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            buffer.insert(buffer.end(), (char*)&pBatch[i].prop, (char*)&pBatch[i].prop + sizeof(vr::ETrackedDeviceProperty));
            buffer.insert(buffer.end(), (char*)&pBatch[i].writeType, (char*)&pBatch[i].writeType + sizeof(vr::EPropertyWriteType));
            buffer.insert(buffer.end(), (char*)&pBatch[i].unTag, (char*)&pBatch[i].unTag + sizeof(vr::PropertyTypeTag_t));
            buffer.insert(buffer.end(), (char*)&pBatch[i].unBufferSize, (char*)&pBatch[i].unBufferSize + sizeof(uint32_t));
            if (pBatch[i].pvBuffer && pBatch[i].unBufferSize > 0) {
                buffer.insert(buffer.end(), (char*)pBatch[i].pvBuffer, (char*)pBatch[i].pvBuffer + pBatch[i].unBufferSize);
            }

            // Log property being written
            //std::cout << "Writing property: " << pBatch[i].prop << " with buffer size: " << pBatch[i].unBufferSize << std::endl;
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), "WritePropertyBatch", RpcValue(ulContainerHandle), RpcValue(buffer.data(), buffer.size()));

        //std::cout << "WritePropertyBatch RPC returned " << (result.isPointer() ? "pointer" : "non-pointer") << std::endl;

        if (!result.isPointer()) return vr::TrackedProp_IPCReadFailure;

        auto response_data = result.asPointer();
        const char* ptr = response_data.first;
        vr::ETrackedPropertyError overallError;
        memcpy(&overallError, ptr, sizeof(vr::ETrackedPropertyError));
        ptr += sizeof(vr::ETrackedPropertyError);
        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            memcpy(&pBatch[i].eError, ptr, sizeof(vr::ETrackedPropertyError));
            ptr += sizeof(vr::ETrackedPropertyError);
        }
        return overallError;
    }
    return real_properties_ ? real_properties_->WritePropertyBatch(ulContainerHandle, pBatch, unBatchEntryCount) : vr::TrackedProp_InvalidOperation;
}
const char *RpcProperties::GetPropErrorNameFromEnum(vr::ETrackedPropertyError error) {
    if (IsProxy()) {
        static std::string error_name;
        error_name = RpcSystem::CallMethod(GetId(), "GetPropErrorNameFromEnum", RpcValue((int)error)).asString();
        return error_name.c_str();
    } else {
        return real_properties_->GetPropErrorNameFromEnum(error);
    }
}
vr::PropertyContainerHandle_t RpcProperties::TrackedDeviceToPropertyContainer(vr::TrackedDeviceIndex_t nDevice) {
    if (IsProxy()) {
        auto handle = (vr::PropertyContainerHandle_t)RpcSystem::CallMethod(GetId(), "TrackedDeviceToPropertyContainer", RpcValue((int)nDevice)).asUint64();
        return handle;
    } else {
        return real_properties_->TrackedDeviceToPropertyContainer(nDevice);
    }
}

// --- RpcResources ---

RpcResources::RpcResources(vr::IVRResources* real) : RpcObject(), real_resources_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "LoadSharedResource", [this](const auto& args) {
            char buffer[4096]; // A reasonable max size for a resource
            const std::string resource_name = args[0].asString();
            uint32_t result = this->LoadSharedResource(resource_name.c_str(), buffer, sizeof(buffer));
            if (result > 0 && result <= sizeof(buffer)) {
                return RpcValue(std::string(buffer));
            }
            return RpcValue();
        });
        RpcSystem::RegisterFunction(prefix + "GetResourceFullPath", [this](const auto& args) {
            char buffer[4096];
            const std::string resource_name = args[0].asString();
            const std::string resource_type = args[1].asString();
            uint32_t result = this->GetResourceFullPath(resource_name.c_str(), resource_type.c_str(), buffer, sizeof(buffer));
            if (result > 0 && result <= sizeof(buffer)) {
                return RpcValue(std::string(buffer));
            }
            return RpcValue();
        });
    }
}
RpcResources::RpcResources(RpcObjectId id) : RpcObject(id) {}
RpcResources::~RpcResources() {}

const std::string& RpcResources::GetRpcClassName() const {
    static const std::string name = "IVRResources";
    return name;
}

uint32_t RpcResources::LoadSharedResource(const char *pchResourceName, char *pchBuffer, uint32_t unBufferLen) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "LoadSharedResource", RpcValue(std::string(pchResourceName)));
        if (result.isString()) {
            std::string data = result.asString();
            if (pchBuffer && unBufferLen > 0) {
                strncpy(pchBuffer, data.c_str(), unBufferLen);
                pchBuffer[std::min((size_t)unBufferLen - 1, data.length())] = '\0';
            }
            return data.length() + 1;
        }
        return 0;
    } else {
        return real_resources_->LoadSharedResource(pchResourceName, pchBuffer, unBufferLen);
    }
}

uint32_t RpcResources::GetResourceFullPath(const char *pchResourceName, const char *pchResourceTypeDirectory, char *pchPathBuffer, uint32_t unBufferLen) {
    if (IsProxy()) {
        RpcValue result = RpcSystem::CallMethod(GetId(), "GetResourceFullPath", RpcValue(std::string(pchResourceName)), RpcValue(std::string(pchResourceTypeDirectory)));
        if (result.isString()) {
            std::string data = result.asString();
            if (pchPathBuffer && unBufferLen > 0) {
                strncpy(pchPathBuffer, data.c_str(), unBufferLen);
                pchPathBuffer[std::min((size_t)unBufferLen - 1, data.length())] = '\0';
            }
            return data.length() + 1;
        }
        return 0;
    } else {
        return real_resources_->GetResourceFullPath(pchResourceName, pchResourceTypeDirectory, pchPathBuffer, unBufferLen);
    }
}
