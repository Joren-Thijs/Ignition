#include "vr_rpc_interfaces.h"
#include <iostream>

// --- RpcDriverLog ---

RpcDriverLog::RpcDriverLog(vr::IVRDriverLog* real) : RpcObject(), real_log_(real) {
    if (!IsProxy()) {
        std::string prefix = std::to_string(GetId()) + ".";
        RpcSystem::RegisterFunction(prefix + "Log", [this](const auto& args){
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
    }
    else {
        real_log_->Log(pchLogMessage);
    }
}
