# Ignition

Allows you to run Windows-only SteamVR drivers on Linux, using Wine/Proton.

# How to use

## SteamOS
For SteamOS you can use the included [install script](setup/install-ignition-steam-os.sh).

## Other Distros

> [!NOTE]
> If you want to get started with Playstation VR2 on Ignition, go to the [Linux support](https://github.com/BnuuySolutions/PSVR2Toolkit/wiki/Linux-support) wiki page for PSVR2Toolkit.

Currently, Ignition is packaged for Linux to run Windows drivers. The Ignition package (named Ignition-Linux-Windows) can be extracted to a folder like `/opt/ignition`, or somewhere that is at least accessible to applications running under the Steam Linux Runtime. `install_ignition.sh <driver path>` will create a new `<driver path>/bin/linux64` directory for SteamVR to load the Windows driver through Ignition. Ignition is configured by `ignition.json`, with the install script automatically filling out the config to run the Windows SteamVR driver under Proton. You must at least install Proton from Steam, preferably **Proton Experimental**. You can use `driver_install.sh` in the `linux64` folder to install the driver for SteamVR to load.

You may also use Ignition on Windows to run Windows drivers on top of it, which is helpful for validating driver behavior due to Ignition. Currently, there is no packaging for Windows, so you must build and set it up yourself.

# Supported Drivers

Currently, Ignition only supports PlayStation VR2 (using [PSVR2Toolkit](https://github.com/BnuuySolutions/PSVR2Toolkit) with the experimental release and some minor modifications). Support for more drivers is something that will hopefully happen over time. Right now, most drivers don't work under Wine/Proton due to no USB support or missing dependencies.

# How it works

There are three main parts to Ignition:

1. **Linux Driver Shim (`driver_ignition`)**: SteamVR on Linux loads Ignition's native Linux library (`.so`) as if it were a standard Linux driver. This proxy forwards API calls and data to and from the Windows Server.
2. **Windows Server (`ignition_server.exe`)**: Upon launch, the Linux shim starts a background Windows server process inside Wine/Proton. This server loads the target Windows driver (`.dll`) and communicates with the VR hardware through Wine/Proton.
3. **IPC Bridge & Shared Memory (`ignition_bridge`)**: An Inter-Process Communication (IPC) layer translates SteamVR API calls between the Linux shim and the Windows server. High-frequency data (such as device tracking, poses, and input events) is passed via shared memory buffers. A Wine DLL (`ignition_bridge.dll`) is loaded by the Windows side, wrapping the POSIX SHM IPC implementation.

### RPC Architecture

Ignition's RPC/IPC system is designed to be low latency and simple as possible.

* **Shared Memory Circular Buffers**: Communication between the Linux driver shim and the Windows server process is established over bi-directional shared memory circular buffers (Client-to-Server and Server-to-Client channels). Buffer access is synchronized using semaphores (or Windows Events) and atomic spinlocks.
* **RPC Layer & Object Proxies**: SteamVR interfaces (such as `IServerTrackedDeviceProvider`, `IVRDriverInput`, `IVRProperties`, `IVRSettings`, etc.) are mirrored across process boundaries using C++ RPC proxy objects derived from `RpcObject`. `RpcObject` is the base class that provides a generic interface for RPC communication.
* **Serialization & Thread Pool**: Method calls and return values are serialized into binary payloads (`RpcSerializer` / `RpcValue`) and pushed through the circular buffers. A dedicated listener thread and worker thread pool process incoming RPC messages, invoking target driver methods and returning results back to the caller via the circular buffer.
