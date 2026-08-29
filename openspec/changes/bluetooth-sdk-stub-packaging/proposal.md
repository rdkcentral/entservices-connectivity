## Why

`runtime-backend-selection` established that the Bluetooth plugin picks its backend at runtime by checking whether `/usr/lib/bluetoothsdk/librdk_bluetooth.so` exists. The main plugin currently links the SDK directly, however, so the ELF loader requires the SDK before that decision can run.

The middleware containing `entservices-connectivity` ships as a **prebuilt IPK shared across all products**. It cannot be rebuilt per device. Today `Bluetooth/CMakeLists.txt` gates the SDK sources behind `find_package(BLUETOOTH_SDK)`, so a middleware build performed in a sysroot without the Bluetooth SDK produces a binary with no SDK backend at all — and even if it did compile, linking `librdk_bluetooth.so` records a `DT_NEEDED` entry that cannot be resolved on a product that lacks the SDK, so the plugin would fail to load before the runtime check ever ran.

SDK availability is rolling out on a staggered schedule that this team does not control. The common middleware IPK must run on every product, including products without the SDK. Build the SDK implementation as a private backend module and load it only after confirming the real SDK marker exists; otherwise use the directly linked BTMgr implementation.

## What Changes

- Build `BtSdkAdapterImpl`, `EventBridge`, `AuthBridge`, and `DeviceTypeClassifier` into a private `libBluetoothSdkBackend.so` module.
- Keep the main plugin free of a `DT_NEEDED` entry for `librdk_bluetooth.so`; it retains the BTMgr implementation and its `libBTMgr.so` dependency.
- When the real SDK marker exists, load the real SDK by its absolute path, then load the private SDK backend and resolve its C factory functions.
- When the marker is absent, do not load either SDK library; select BTMgr.
- Do not modify the SDK-owned recipe, vendor recipes, preferred-provider settings, or BTMgr systemd condition.

## Capabilities

### New Capabilities
- `bluetooth-sdk-module-loading`: The load-on-demand SDK backend module and its factory/lifecycle contract.

### Modified Capabilities
- `bluetooth-backend-selection`: Unchanged in behavior. This change makes its runtime check reachable on non-SDK products and preserves the truthfulness of the path it tests.

## Impact

- `Bluetooth/CMakeLists.txt`: creates the private SDK module and links the main plugin only to BTMgr and `dl`.
- `Bluetooth/BtAdapter.*`: dynamically loads the SDK and module after the marker check, invokes factory functions, and retains test injection.
- `Bluetooth/BtSdkAdapterImpl.*`, `EventBridge.*`, `AuthBridge.*`, and `DeviceTypeClassifier.*`: move from the plugin target to the SDK module target.
- L1/L2 tests retain mock injection and gain loader-failure coverage.

## Non-Goals

- Changing the runtime selection rule established by `runtime-backend-selection`.
- Refactoring the Bluetooth SDK's public headers, which are owned by another team.
- Dynamically loading BTMgr; `libBTMgr.so` is guaranteed on all Bluetooth-enabled products.
- Changing the JSON-RPC surface or plugin API.
