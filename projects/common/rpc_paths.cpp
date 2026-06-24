#include "openvr_driver.h"
#include "rpc_interfaces.h"
#include <algorithm>
#include <vector>
#include <string>

RpcPaths::RpcPaths(vr::IVRPaths* real) : RpcObject(), real_paths_(real) {
    if (!IsProxy()) {
        this->RegisterFunction(RPCFunction_Paths_StringToHandle, [this](const auto& args) {
            std::string path = args[0].asString();
            vr::PathHandle_t handle = 0;
            vr::ETrackedPropertyError err = this->StringToHandle(&handle, path.c_str());
            
            std::vector<char> return_buffer;
            return_buffer.insert(return_buffer.end(), (char*)&err, (char*)&err + sizeof(vr::ETrackedPropertyError));
            return_buffer.insert(return_buffer.end(), (char*)&handle, (char*)&handle + sizeof(vr::PathHandle_t));
            return RpcValue(return_buffer.data(), return_buffer.size());
        });

        this->RegisterFunction(RPCFunction_Paths_HandleToString, [this](const auto& args) {
            vr::PathHandle_t handle = args[0].asUint64();
            uint32_t unBufferSize = (uint32_t)args[1].asInt();

            std::vector<char> temp_buffer(unBufferSize);
            uint32_t unBufferSizeUsed = 0;
            vr::ETrackedPropertyError err = this->HandleToString(handle, temp_buffer.data(), unBufferSize, &unBufferSizeUsed);

            std::vector<char> return_buffer;
            return_buffer.insert(return_buffer.end(), (char*)&err, (char*)&err + sizeof(vr::ETrackedPropertyError));
            return_buffer.insert(return_buffer.end(), (char*)&unBufferSizeUsed, (char*)&unBufferSizeUsed + sizeof(uint32_t));
            if (err == vr::TrackedProp_Success && unBufferSizeUsed > 0) {
                uint32_t bytes_to_send = std::min(unBufferSize, unBufferSizeUsed);
                return_buffer.insert(return_buffer.end(), temp_buffer.begin(), temp_buffer.begin() + bytes_to_send);
            }
            return RpcValue(return_buffer.data(), return_buffer.size());
        });

        this->RegisterFunction(RPCFunction_Paths_ReadPathBatch, [this](const auto& args) {
            vr::PropertyContainerHandle_t ulRootHandle = args[0].asUint64();
            auto batch_data = args[1].asByteArray();
            const char* ptr = batch_data.data();
            const char* end_ptr = ptr + batch_data.size();

            uint32_t unBatchEntryCount;
            if (ptr + sizeof(uint32_t) > end_ptr) return RpcValue();
            memcpy(&unBatchEntryCount, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            std::vector<vr::PathRead_t> batch(unBatchEntryCount);
            std::vector<std::vector<char>> data_buffers(unBatchEntryCount);
            std::vector<std::string> path_strings(unBatchEntryCount);

            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                if (ptr + sizeof(vr::PathHandle_t) + sizeof(uint32_t) + sizeof(vr::PropertyTypeTag_t) + 1 > end_ptr) {
                    return RpcValue();
                }
                memcpy(&batch[i].ulPath, ptr, sizeof(vr::PathHandle_t));
                ptr += sizeof(vr::PathHandle_t);
                memcpy(&batch[i].unBufferSize, ptr, sizeof(uint32_t));
                ptr += sizeof(uint32_t);
                memcpy(&batch[i].unTag, ptr, sizeof(vr::PropertyTypeTag_t));
                ptr += sizeof(vr::PropertyTypeTag_t);

                uint8_t has_path = *ptr++;
                if (has_path) {
                    if (ptr + sizeof(uint32_t) > end_ptr) return RpcValue();
                    uint32_t path_len;
                    memcpy(&path_len, ptr, sizeof(uint32_t));
                    ptr += sizeof(uint32_t);
                    if (ptr + path_len > end_ptr) return RpcValue();
                    path_strings[i].assign(ptr, path_len);
                    batch[i].pszPath = path_strings[i].c_str();
                    ptr += path_len;
                } else {
                    batch[i].pszPath = nullptr;
                }

                if (batch[i].unBufferSize > 0) {
                    data_buffers[i].resize(batch[i].unBufferSize);
                    batch[i].pvBuffer = data_buffers[i].data();
                } else {
                    batch[i].pvBuffer = nullptr;
                }
            }

            vr::ETrackedPropertyError overallError = this->ReadPathBatch(ulRootHandle, batch.data(), unBatchEntryCount);

            std::vector<char> return_buffer;
            return_buffer.insert(return_buffer.end(), (char*)&overallError, (char*)&overallError + sizeof(overallError));
            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].ulPath, (char*)&batch[i].ulPath + sizeof(vr::PathHandle_t));
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].unTag, (char*)&batch[i].unTag + sizeof(vr::PropertyTypeTag_t));
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].unRequiredBufferSize, (char*)&batch[i].unRequiredBufferSize + sizeof(uint32_t));
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].eError, (char*)&batch[i].eError + sizeof(vr::ETrackedPropertyError));

                if (batch[i].eError == vr::TrackedProp_Success && batch[i].unRequiredBufferSize > 0) {
                    uint32_t bytes_to_send = std::min(batch[i].unBufferSize, batch[i].unRequiredBufferSize);
                    if (bytes_to_send > 0 && batch[i].pvBuffer) {
                        return_buffer.insert(return_buffer.end(), (char*)batch[i].pvBuffer, (char*)batch[i].pvBuffer + bytes_to_send);
                    }
                }
            }
            return RpcValue(return_buffer.data(), return_buffer.size());
        });

        this->RegisterFunction(RPCFunction_Paths_WritePathBatch, [this](const auto& args) {
            vr::PropertyContainerHandle_t ulRootHandle = args[0].asUint64();
            auto batch_data = args[1].asByteArray();
            const char* ptr = batch_data.data();
            const char* end_ptr = ptr + batch_data.size();

            uint32_t unBatchEntryCount;
            if (ptr + sizeof(uint32_t) > end_ptr) return RpcValue();
            memcpy(&unBatchEntryCount, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            std::vector<vr::PathWrite_t> batch(unBatchEntryCount);
            std::vector<std::vector<char>> data_buffers(unBatchEntryCount);
            std::vector<std::string> path_strings(unBatchEntryCount);

            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                if (ptr + sizeof(vr::PathHandle_t) + sizeof(vr::EPropertyWriteType) + sizeof(vr::ETrackedPropertyError) + sizeof(vr::PropertyTypeTag_t) + sizeof(uint32_t) > end_ptr) {
                    return RpcValue();
                }
                memcpy(&batch[i].ulPath, ptr, sizeof(vr::PathHandle_t));
                ptr += sizeof(vr::PathHandle_t);
                memcpy(&batch[i].writeType, ptr, sizeof(vr::EPropertyWriteType));
                ptr += sizeof(vr::EPropertyWriteType);
                memcpy(&batch[i].eSetError, ptr, sizeof(vr::ETrackedPropertyError));
                ptr += sizeof(vr::ETrackedPropertyError);
                memcpy(&batch[i].unTag, ptr, sizeof(vr::PropertyTypeTag_t));
                ptr += sizeof(vr::PropertyTypeTag_t);
                memcpy(&batch[i].unBufferSize, ptr, sizeof(uint32_t));
                ptr += sizeof(uint32_t);

                if (batch[i].unBufferSize > 0) {
                    if (ptr + batch[i].unBufferSize > end_ptr) return RpcValue();
                    data_buffers[i].assign(ptr, ptr + batch[i].unBufferSize);
                    batch[i].pvBuffer = data_buffers[i].data();
                    ptr += batch[i].unBufferSize;
                } else {
                    batch[i].pvBuffer = nullptr;
                }

                if (ptr + 1 > end_ptr) return RpcValue();
                uint8_t has_path = *ptr++;
                if (has_path) {
                    if (ptr + sizeof(uint32_t) > end_ptr) return RpcValue();
                    uint32_t path_len;
                    memcpy(&path_len, ptr, sizeof(uint32_t));
                    ptr += sizeof(uint32_t);
                    if (ptr + path_len > end_ptr) return RpcValue();
                    path_strings[i].assign(ptr, path_len);
                    batch[i].pszPath = path_strings[i].c_str();
                    ptr += path_len;
                } else {
                    batch[i].pszPath = nullptr;
                }
            }

            vr::ETrackedPropertyError overallError = this->WritePathBatch(ulRootHandle, batch.data(), unBatchEntryCount);

            std::vector<char> return_buffer;
            return_buffer.insert(return_buffer.end(), (char*)&overallError, (char*)&overallError + sizeof(overallError));
            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].eSetError, (char*)&batch[i].eSetError + sizeof(vr::ETrackedPropertyError));
                return_buffer.insert(return_buffer.end(), (char*)&batch[i].eError, (char*)&batch[i].eError + sizeof(vr::ETrackedPropertyError));
            }
            return RpcValue(return_buffer.data(), return_buffer.size());
        });
    }
}

RpcPaths::RpcPaths(RpcObjectId id) : RpcObject(id) {}
RpcPaths::~RpcPaths() {}

RpcClassEnum RpcPaths::GetRpcClassId() const {
    return RPCClassPaths;
}

vr::ETrackedPropertyError RpcPaths::StringToHandle(vr::PathHandle_t *pHandle, const char *pchPath) {

    auto execute_string_to_handle = [&]() -> vr::ETrackedPropertyError {
        if (IsProxy()) {
            std::string path_str = pchPath ? pchPath : "";
            RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Paths_StringToHandle, RpcValue(path_str));
            if (!result.isByteArray()) {
                return vr::TrackedProp_IPCReadFailure;
            }

            auto response_data = result.asByteArray();
            if (response_data.size() < sizeof(vr::ETrackedPropertyError) + sizeof(vr::PathHandle_t)) {
                return vr::TrackedProp_IPCReadFailure;
            }

            const char* ptr = response_data.data();
            vr::ETrackedPropertyError err;
            memcpy(&err, ptr, sizeof(vr::ETrackedPropertyError));
            ptr += sizeof(vr::ETrackedPropertyError);

            vr::PathHandle_t handle;
            memcpy(&handle, ptr, sizeof(vr::PathHandle_t));

            if (pHandle) {
                *pHandle = handle;
            }
            return err;
        } else {
            return real_paths_->StringToHandle(pHandle, pchPath);
        }
    };

    vr::ETrackedPropertyError err = execute_string_to_handle();



    return err;
}

vr::ETrackedPropertyError RpcPaths::HandleToString(vr::PathHandle_t pHandle, const char *pchBuffer, uint32_t unBufferSize, uint32_t *punBufferSizeUsed) {

    auto execute_handle_to_string = [&]() -> vr::ETrackedPropertyError {
        if (IsProxy()) {
            RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Paths_HandleToString, RpcValue(pHandle), RpcValue((int)unBufferSize));
            if (!result.isByteArray()) {
                return vr::TrackedProp_IPCReadFailure;
            }

            auto response_data = result.asByteArray();
            if (response_data.size() < sizeof(vr::ETrackedPropertyError) + sizeof(uint32_t)) {
                return vr::TrackedProp_IPCReadFailure;
            }

            const char* ptr = response_data.data();
            vr::ETrackedPropertyError err;
            memcpy(&err, ptr, sizeof(vr::ETrackedPropertyError));
            ptr += sizeof(vr::ETrackedPropertyError);

            uint32_t unBufferSizeUsed = 0;
            memcpy(&unBufferSizeUsed, ptr, sizeof(uint32_t));
            ptr += sizeof(uint32_t);

            if (punBufferSizeUsed) {
                *punBufferSizeUsed = unBufferSizeUsed;
            }

            if (err == vr::TrackedProp_Success && unBufferSizeUsed > 0 && pchBuffer && unBufferSize > 0) {
                uint32_t bytes_to_copy = std::min(unBufferSize, unBufferSizeUsed);
                if (response_data.size() < sizeof(vr::ETrackedPropertyError) + sizeof(uint32_t) + bytes_to_copy) {
                    return vr::TrackedProp_IPCReadFailure;
                }
                memcpy((void*)pchBuffer, ptr, bytes_to_copy);
            }

            return err;
        } else {
            return real_paths_->HandleToString(pHandle, pchBuffer, unBufferSize, punBufferSizeUsed);
        }
    };

    vr::ETrackedPropertyError err = execute_handle_to_string();

    return err;
}

vr::ETrackedPropertyError RpcPaths::ReadPathBatch(vr::PropertyContainerHandle_t ulRootHandle, vr::PathRead_t *pBatch, uint32_t unBatchEntryCount) {

    auto execute_read = [&]() -> vr::ETrackedPropertyError {
        if (IsProxy()) {
            std::vector<char> request_buffer;
            request_buffer.insert(request_buffer.end(), (char*)&unBatchEntryCount, (char*)&unBatchEntryCount + sizeof(uint32_t));
            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].ulPath, (char*)&pBatch[i].ulPath + sizeof(vr::PathHandle_t));
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].unBufferSize, (char*)&pBatch[i].unBufferSize + sizeof(uint32_t));
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].unTag, (char*)&pBatch[i].unTag + sizeof(vr::PropertyTypeTag_t));

                uint8_t has_path = (pBatch[i].pszPath != nullptr) ? 1 : 0;
                request_buffer.insert(request_buffer.end(), (char*)&has_path, (char*)&has_path + 1);
                if (has_path) {
                    std::string path_str(pBatch[i].pszPath);
                    uint32_t path_len = path_str.size();
                    request_buffer.insert(request_buffer.end(), (char*)&path_len, (char*)&path_len + sizeof(uint32_t));
                    request_buffer.insert(request_buffer.end(), path_str.begin(), path_str.end());
                }
            }

            RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Paths_ReadPathBatch, RpcValue(ulRootHandle), RpcValue(request_buffer.data(), request_buffer.size()));
            if (!result.isByteArray()) {
                return vr::TrackedProp_IPCReadFailure;
            }

            auto response_data = result.asByteArray();
            const char* ptr = response_data.data();
            const char* end_ptr = ptr + response_data.size();

            if (ptr + sizeof(vr::ETrackedPropertyError) > end_ptr) {
                return vr::TrackedProp_IPCReadFailure;
            }

            vr::ETrackedPropertyError overallError;
            memcpy(&overallError, ptr, sizeof(vr::ETrackedPropertyError));
            ptr += sizeof(vr::ETrackedPropertyError);

            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                size_t min_header_size = sizeof(vr::PathHandle_t) + sizeof(vr::PropertyTypeTag_t) + sizeof(uint32_t) + sizeof(vr::ETrackedPropertyError);
                if (ptr + min_header_size > end_ptr) {
                    return vr::TrackedProp_IPCReadFailure;
                }

                memcpy(&pBatch[i].ulPath, ptr, sizeof(vr::PathHandle_t));
                ptr += sizeof(vr::PathHandle_t);
                memcpy(&pBatch[i].unTag, ptr, sizeof(vr::PropertyTypeTag_t));
                ptr += sizeof(vr::PropertyTypeTag_t);
                memcpy(&pBatch[i].unRequiredBufferSize, ptr, sizeof(uint32_t));
                ptr += sizeof(uint32_t);
                memcpy(&pBatch[i].eError, ptr, sizeof(vr::ETrackedPropertyError));
                ptr += sizeof(vr::ETrackedPropertyError);

                if (pBatch[i].eError == vr::TrackedProp_Success && pBatch[i].unRequiredBufferSize > 0) {
                    uint32_t bytes_to_copy = std::min(pBatch[i].unBufferSize, pBatch[i].unRequiredBufferSize);
                    if (bytes_to_copy > 0) {
                        if (ptr + bytes_to_copy > end_ptr) {
                            return vr::TrackedProp_IPCReadFailure;
                        }
                        if (pBatch[i].pvBuffer) {
                            memcpy(pBatch[i].pvBuffer, ptr, bytes_to_copy);
                        }
                        ptr += bytes_to_copy;
                    }
                }
            }

            return overallError;
        } else {
            return real_paths_->ReadPathBatch(ulRootHandle, pBatch, unBatchEntryCount);
        }
    };

    vr::ETrackedPropertyError overallError = execute_read();



    return overallError;
}

vr::ETrackedPropertyError RpcPaths::WritePathBatch(vr::PropertyContainerHandle_t ulRootHandle, vr::PathWrite_t *pBatch, uint32_t unBatchEntryCount) {

    auto execute_write = [&]() -> vr::ETrackedPropertyError {
        if (IsProxy()) {
            std::vector<char> request_buffer;
            request_buffer.insert(request_buffer.end(), (char*)&unBatchEntryCount, (char*)&unBatchEntryCount + sizeof(uint32_t));
            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].ulPath, (char*)&pBatch[i].ulPath + sizeof(vr::PathHandle_t));
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].writeType, (char*)&pBatch[i].writeType + sizeof(vr::EPropertyWriteType));
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].eSetError, (char*)&pBatch[i].eSetError + sizeof(vr::ETrackedPropertyError));
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].unTag, (char*)&pBatch[i].unTag + sizeof(vr::PropertyTypeTag_t));
                request_buffer.insert(request_buffer.end(), (char*)&pBatch[i].unBufferSize, (char*)&pBatch[i].unBufferSize + sizeof(uint32_t));

                if (pBatch[i].unBufferSize > 0 && pBatch[i].pvBuffer) {
                    request_buffer.insert(request_buffer.end(), (char*)pBatch[i].pvBuffer, (char*)pBatch[i].pvBuffer + pBatch[i].unBufferSize);
                }

                uint8_t has_path = (pBatch[i].pszPath != nullptr) ? 1 : 0;
                request_buffer.insert(request_buffer.end(), (char*)&has_path, (char*)&has_path + 1);
                if (has_path) {
                    std::string path_str(pBatch[i].pszPath);
                    uint32_t path_len = path_str.size();
                    request_buffer.insert(request_buffer.end(), (char*)&path_len, (char*)&path_len + sizeof(uint32_t));
                    request_buffer.insert(request_buffer.end(), path_str.begin(), path_str.end());
                }
            }

            RpcValue result = RpcSystem::CallMethod(GetId(), RPCFunction_Paths_WritePathBatch, RpcValue(ulRootHandle), RpcValue(request_buffer.data(), request_buffer.size()));
            if (!result.isByteArray()) {
                return vr::TrackedProp_IPCReadFailure;
            }

            auto response_data = result.asByteArray();
            if (response_data.size() < sizeof(vr::ETrackedPropertyError) + unBatchEntryCount * (sizeof(vr::ETrackedPropertyError) * 2)) {
                return vr::TrackedProp_IPCReadFailure;
            }

            const char* ptr = response_data.data();
            vr::ETrackedPropertyError overallError;
            memcpy(&overallError, ptr, sizeof(vr::ETrackedPropertyError));
            ptr += sizeof(vr::ETrackedPropertyError);

            for (uint32_t i = 0; i < unBatchEntryCount; ++i) {
                memcpy(&pBatch[i].eSetError, ptr, sizeof(vr::ETrackedPropertyError));
                ptr += sizeof(vr::ETrackedPropertyError);
                memcpy(&pBatch[i].eError, ptr, sizeof(vr::ETrackedPropertyError));
                ptr += sizeof(vr::ETrackedPropertyError);
            }

            return overallError;
        } else {
            return real_paths_->WritePathBatch(ulRootHandle, pBatch, unBatchEntryCount);
        }
    };

    vr::ETrackedPropertyError overallError = execute_write();



    return overallError;
}
