#include "vr_rpc_interfaces.h"
#include <iostream>
#include <vector>
#include <algorithm>

// Helper struct to manage the memory for a property batch read result.
struct PropertyReadBatchResult {
    vr::ETrackedPropertyError overallError = vr::TrackedProp_Success;
    std::vector<vr::PropertyRead_t> batch;
    std::vector<char> data_buffer;
};

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
        }

        RpcValue result = RpcSystem::CallMethod(GetId(), "WritePropertyBatch", RpcValue(ulContainerHandle), RpcValue(buffer.data(), buffer.size()));

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
    else {
        return real_properties_->WritePropertyBatch(ulContainerHandle, pBatch, unBatchEntryCount);
    }
}
const char *RpcProperties::GetPropErrorNameFromEnum(vr::ETrackedPropertyError error) {
    if (IsProxy()) {
        static std::string error_name;
        error_name = RpcSystem::CallMethod(GetId(), "GetPropErrorNameFromEnum", RpcValue((int)error)).asString();
        return error_name.c_str();
    }
    else {
        return real_properties_->GetPropErrorNameFromEnum(error);
    }
}
vr::PropertyContainerHandle_t RpcProperties::TrackedDeviceToPropertyContainer(vr::TrackedDeviceIndex_t nDevice) {
    if (IsProxy()) {
        auto handle = (vr::PropertyContainerHandle_t)RpcSystem::CallMethod(GetId(), "TrackedDeviceToPropertyContainer", RpcValue((int)nDevice)).asUint64();
        return handle;
    }
    else {
        return real_properties_->TrackedDeviceToPropertyContainer(nDevice);
    }
}
