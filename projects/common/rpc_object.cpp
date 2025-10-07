#include "rpc_core.h"

// --- RpcObject Implementation ---
RpcObject::RpcObject() : is_proxy_(false)
{
    object_id_ = RpcSystem::GenerateObjectId();
    RpcSystem::RegisterLocalObject(this);
}

RpcObject::RpcObject(RpcObjectId id) : object_id_(id), is_proxy_(true) {}

RpcObject::~RpcObject() {
    if (!is_proxy_) {
        RpcSystem::UnregisterLocalObject(object_id_);
    }
}