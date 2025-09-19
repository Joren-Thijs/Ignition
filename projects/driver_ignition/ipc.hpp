#pragma once

#include <openvr.hpp>

#include <memory>

namespace ipc {
    class IpcClient;

    struct Command {
    public:
        enum Type {
            IServerTrackedDeviceProvider_Init,
            IServerTrackedDeviceProvider_ShouldBlockStandbyMode,
        };

        union Payload {
            struct {
            } IServerTrackedDeviceProvider_Init;

            struct {
            } IServerTrackedDeviceProvider_ShouldBlockStandbyMode;
        };

        Type type;
        Payload payload;
    };

    class InterfaceHandle {
    private:
        typedef uint64_t interface_handle_t;

        interface_handle_t handle;
        std::shared_ptr<IpcClient> client;

    public:
        //! Makes the IPC call with the payload, returning the OpenVR error code that the server's OpenVR call returned
        int MakeCallError(Command command);

        //! Makes the IPC call with the payload, returning the boolean that the server's OpenVR call returned
        bool MakeCallBool(Command command);

        ~InterfaceHandle();
    };

    class IpcClient {
    public:
        //! Gets the opaque handle for the interface from the server
        InterfaceHandle GetInterface(const char *pInterfaceName, int *pReturnCode);
    };
}
