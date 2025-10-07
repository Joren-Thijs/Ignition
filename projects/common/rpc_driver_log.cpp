#include "rpc_interfaces.h"
#include <iostream>

// --- RpcDriverLog ---

RpcDriverLog::RpcDriverLog(vr::IVRDriverLog* real) : RpcObject(), real_log_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        this->RegisterFunction(RPCFunction_DriverLog_Log, [this](const auto& args) {
            this->Log(args[0].asString().c_str());
            return RpcValue();
        });
    }
}

RpcDriverLog::RpcDriverLog(RpcObjectId id) : RpcObject(id) {}

RpcDriverLog::~RpcDriverLog() {}

RpcClassEnum RpcDriverLog::GetRpcClassId() const {
    return RPCClassDriverLog;
}

void RpcDriverLog::Log(const char *pchLogMessage) {
    if (IsProxy()) {
        std::cout << "DriverLog: " << pchLogMessage << std::endl;
        RpcSystem::CallMethod(GetId(), RPCFunction_DriverLog_Log, RpcValue(std::string(pchLogMessage)));
    }
    else {
        real_log_->Log(pchLogMessage);
    }
}
