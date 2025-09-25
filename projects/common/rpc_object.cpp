#include "rpc_core.h"

// --- RpcObject Implementation ---
RpcObject::RpcObject() : is_proxy_(false) {
    object_id_ = RpcSystem::GetInstance()._GenerateObjectId();
    RpcSystem::GetInstance()._RegisterLocalObject(this);
}

RpcObject::RpcObject(RpcObjectId id) : object_id_(id), is_proxy_(true) {}

RpcObject::~RpcObject() {
    if (!is_proxy_) {
        RpcSystem::GetInstance()._UnregisterLocalObject(object_id_);
    }
}