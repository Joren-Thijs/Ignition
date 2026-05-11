#include "openvr_driver.h"
#include "rpc_interfaces.h"

#ifdef _WIN32
#include <wine_utils.h>
#endif

#include <algorithm>
#include <vector>

// Helper struct to manage the memory for a property batch read result.
struct PropertyReadBatchResult {
    vr::ETrackedPropertyError overallError = vr::TrackedProp_Success;
    std::vector<vr::PropertyRead_t> batch;
    std::vector<char> data_buffer;
};

// --- RpcProperties ---

RpcProperties::RpcProperties(vr::IVRProperties* real) : RpcObject(), real_properties_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_Properties_GetPropErrorNameFromEnum, [this](const auto& args) {
            const char* result_cstr = this->GetPropErrorNameFromEnum((vr::ETrackedPropertyError)args[0].asInt());
            std::string result = result_cstr ? result_cstr : "";
            return RpcValue(result);
        });
        
        this->RegisterFunction(RPCFunction_Properties_TrackedDeviceToPropertyContainer, [this](const auto& args) {
            vr::PropertyContainerHandle_t handle = this->TrackedDeviceToPropertyContainer((vr::TrackedDeviceIndex_t)args[0].asInt());
            return RpcValue(static_cast<uint64_t>(handle));
        });

        this->RegisterFunction(RPCFunction_Properties_WritePropertyBatch, [this](const auto& args) {
            vr::PropertyContainerHandle_t ulContainerHandle = args[0].asUint64();
            auto batch_data = args[1].asByteArray();
            const char* ptr = batch_data.data();

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

            vr::ETrackedPropertyError overallError = this->WritePropertyBatch(ulContainerHandle, batch.data(), unBatchEntryCount);

            std::vector<char> return_buffer;
            return_buffer.insert(return_buffer.end(), (char*)&overallError, (char*)&overallError + sizeof(overallError));
            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].eError, (char*)&batch[i].eError + sizeof(vr::ETrackedPropertyError));
            }
            return RpcValue(return_buffer.data(), return_buffer.size());
        });

        this->RegisterFunction(RPCFunction_Properties_ReadPropertyBatch, [this](const auto& args) {
            vr::PropertyContainerHandle_t ulContainerHandle = args[0].asUint64();
            auto batch_data = args[1].asByteArray();
            const char* ptr = batch_data.data();

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

            vr::ETrackedPropertyError overallError = this->ReadPropertyBatch(ulContainerHandle, batch.data(), unBatchEntryCount);

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

RpcClassEnum RpcProperties::GetRpcClassId() const {
    return RPCClassProperties;
}

vr::ETrackedPropertyError RpcProperties::ReadPropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyRead_t *pBatch, uint32_t unBatchEntryCount) {
    if (IsProxy()) {
        std::vector<char> request_buffer;
        request_buffer.insert(request_buffer.end(), (char*)&unBatchEntryCount, (char*)&unBatchEntryCount + sizeof(uint32_t));
        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            uint32_t requestedBufferSize = pBatch[i].unBufferSize;
#ifdef _WIN32
            if (pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_UserConfigPath_String
                || pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_InstallPath_String
                || pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_DriverProvidedChaperonePath_String) {
                if (IsRunningInWine()) {
                    // We will have to modify the path, so this means making the buffer max size.
                    // That so we can run the conversion and do our own size check on the converted path.
                    requestedBufferSize = vr::k_unMaxPropertyStringSize;
                }
            }
#endif
            request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].prop, (char*)&pBatch[i].prop + sizeof(vr::ETrackedDeviceProperty));
            request_buffer.insert(request_buffer.end(), (char*)&requestedBufferSize, (char*)&requestedBufferSize + sizeof(uint32_t));
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Properties_ReadPropertyBatch, RpcValue(ulContainerHandle), RpcValue(request_buffer.data(), request_buffer.size()));

        if (!result.isByteArray()) {
            return vr::TrackedProp_IPCReadFailure;
        }

        auto response_data = result.asByteArray();
        
        const char* ptr = response_data.data();
        const char* end_ptr = ptr + response_data.size();

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

            if (pBatch[i].unRequiredBufferSize > 0) {
                uint32_t bytesToCopy = std::min(pBatch[i].unBufferSize, pBatch[i].unRequiredBufferSize);
                uint32_t remoteBufferSize = pBatch[i].unRequiredBufferSize;

                if (bytesToCopy > 0) {
                    if (pBatch[i].pvBuffer) {
                        memcpy(pBatch[i].pvBuffer, current_data_ptr, bytesToCopy);
                    }
#ifdef _WIN32
                    if (pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_UserConfigPath_String
                        || pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_InstallPath_String
                        || pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_DriverProvidedChaperonePath_String) {
                        if (IsRunningInWine()) {
                            // Turn Unix path into Windows (DOS) path that the Windows SteamVR driver expects
                            std::string path_str((char*)current_data_ptr, bytesToCopy);
                            std::string windows_path = WineGetDosFileName(path_str);

                            // Append null terminator
                            windows_path.push_back('\0');
                            
                            pBatch[i].unRequiredBufferSize = static_cast<uint32_t>(windows_path.size());
                            bytesToCopy = std::min(pBatch[i].unBufferSize, pBatch[i].unRequiredBufferSize);
                            if (pBatch[i].pvBuffer) {
                                memcpy(pBatch[i].pvBuffer, windows_path.c_str(), windows_path.size());
                            }

                            // Set error if we can't copy all data into the buffer
                            if (pBatch[i].unBufferSize < pBatch[i].unRequiredBufferSize) {
                                pBatch[i].eError = vr::TrackedProp_BufferTooSmall;
                            }
                        }
                    }
#endif
                }
                current_data_ptr += remoteBufferSize;
            }
        }
        return batchResult.overallError;
    }
    else {
        return real_properties_->ReadPropertyBatch(ulContainerHandle, pBatch, unBatchEntryCount);
    }
}
vr::ETrackedPropertyError RpcProperties::WritePropertyBatch(vr::PropertyContainerHandle_t ulContainerHandle, vr::PropertyWrite_t *pBatch, uint32_t unBatchEntryCount) {
    if (IsProxy()) {
        std::vector<char> buffer;
        buffer.insert(buffer.end(), (char*)&unBatchEntryCount, (char*)&unBatchEntryCount + sizeof(uint32_t));
        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            
            if (pBatch[i].pvBuffer && pBatch[i].unBufferSize > 0) {
                char* bufferStart = (char*)pBatch[i].pvBuffer;
                char* bufferEnd = bufferStart + pBatch[i].unBufferSize;
#ifdef _WIN32
                std::string unix_path; // Needs to persist until contents are written to buffer
                if (pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_UserConfigPath_String
                    || pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_InstallPath_String
                    || pBatch[i].prop == vr::ETrackedDeviceProperty::Prop_DriverProvidedChaperonePath_String) {
                    if (IsRunningInWine()) {
                        // Turn Windows (DOS) path into Unix path that Linux SteamVR expects
                        std::string path_str(bufferStart, pBatch[i].unBufferSize);
                        unix_path = WineGetUnixFileName(path_str);

                        // Append null terminator
                        unix_path.push_back('\0');
                        
                        bufferStart = (char*)unix_path.c_str();
                        bufferEnd = bufferStart + unix_path.size();
                    }
                }
#endif
                uint32_t bufferSize = bufferEnd - bufferStart;
                
                buffer.insert(buffer.end(), (char*)&pBatch[i].prop, (char*)&pBatch[i].prop + sizeof(vr::ETrackedDeviceProperty));
                buffer.insert(buffer.end(), (char*)&pBatch[i].writeType, (char*)&pBatch[i].writeType + sizeof(vr::EPropertyWriteType));
                buffer.insert(buffer.end(), (char*)&pBatch[i].unTag, (char*)&pBatch[i].unTag + sizeof(vr::PropertyTypeTag_t));
                buffer.insert(buffer.end(), (char*)&bufferSize, (char*)&bufferSize + sizeof(uint32_t));
                buffer.insert(buffer.end(), bufferStart, bufferEnd);
            }
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Properties_WritePropertyBatch, RpcValue(ulContainerHandle), RpcValue(buffer.data(), buffer.size()));

        if (!result.isByteArray()) return vr::TrackedProp_IPCReadFailure;

        auto response_data = result.asByteArray();
        const char* ptr = response_data.data();
        vr::ETrackedPropertyError overallError;
        memcpy(&overallError, ptr, sizeof(vr::ETrackedPropertyError));
        ptr += sizeof(vr::ETrackedPropertyError);
        for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
            memcpy(&pBatch[i].eError, ptr, sizeof(vr::ETrackedPropertyError));
            ptr += sizeof(vr::ETrackedPropertyError);
        }

        return overallError;
    }
    else {
        return real_properties_->WritePropertyBatch(ulContainerHandle, pBatch, unBatchEntryCount);
    }
}
const char *RpcProperties::GetPropErrorNameFromEnum(vr::ETrackedPropertyError error) {
    if (IsProxy()) {
        static std::string error_name;
        error_name = RpcSystem::CallMethod(GetId(), RPCFunction_Properties_GetPropErrorNameFromEnum, RpcValue((int)error)).asString();
        return error_name.c_str();
    }
    else {
        return real_properties_->GetPropErrorNameFromEnum(error);
    }
}
vr::PropertyContainerHandle_t RpcProperties::TrackedDeviceToPropertyContainer(vr::TrackedDeviceIndex_t nDevice) {
    if (IsProxy()) {
        auto handle = (vr::PropertyContainerHandle_t)RpcSystem::CallMethod(GetId(), RPCFunction_Properties_TrackedDeviceToPropertyContainer, RpcValue((int)nDevice)).asUint64();
        return handle;
    }
    else {
        return real_properties_->TrackedDeviceToPropertyContainer(nDevice);
    }
}
