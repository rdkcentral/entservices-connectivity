## Why

`runtime-backend-selection` established that the Bluetooth plugin picks its backend at runtime by checking whether `/usr/lib/bluetoothsdk/librdk_bluetooth.so` exists. That decision is sound, but it is not currently reachable on the products that need it most.

The middleware containing `entservices-connectivity` ships as a **prebuilt IPK shared across all products**. It cannot be rebuilt per device. Today `Bluetooth/CMakeLists.txt` gates the SDK sources behind `find_package(BLUETOOTH_SDK)`, so a middleware build performed in a sysroot without the Bluetooth SDK produces a binary with no SDK backend at all — and even if it did compile, linking `librdk_bluetooth.so` records a `DT_NEEDED` entry that cannot be resolved on a product that lacks the SDK, so the plugin would fail to load before the runtime check ever ran.

SDK availability is rolling out on a staggered schedule that this team does not control. Every product is expected to have the SDK eventually, so what is needed is a deliberately temporary bridge: one middleware binary that links the SDK unconditionally, runs the SDK backend where the real SDK is installed, and runs the existing BTMgr backend everywhere else.

This mirrors the MotionDetector precedent, where `virtual/vendor-motiondetector-hal` is satisfied by either the real vendor HAL or `motiondetector-hal-noop`, both installing the same `libmd-hal.so` SONAME, with `PREFERRED_PROVIDER` selecting per product.

## What Changes

- Introduce a no-op stub implementation of the Bluetooth SDK that is ABI-compatible with the real `librdk_bluetooth.so`, satisfying every SDK symbol the plugin references so the middleware can build and link without the real SDK present.
- Add a `virtual/bluetooth-sdk` provider pair — the real `bluetoothsdk` recipe and a new stub recipe built at middleware architecture — selected via `PREFERRED_PROVIDER`, following the MotionDetector convention.
- Install the stub library in the **default library directory**. The real SDK remains at `${libdir}/bluetoothsdk/librdk_bluetooth.so` and gains a symlink in the default library directory. The loader resolves either provider from the standard directory, while the plugin's existence check and the `btmgr.service` start condition continue to test the real-SDK marker path only.
- Confirm and require a matching `SONAME` on both the real SDK library and the stub so the provider swap resolves correctly on device.
- Remove the conditional backend compilation from `Bluetooth/CMakeLists.txt` so the production binary always contains both backends and always links the SDK.
- Establish a maintenance obligation: the stub must define every SDK symbol the plugin references, and must be updated when the plugin's SDK usage grows.

## Capabilities

### New Capabilities
- `bluetooth-backend-packaging`: The build, link, and packaging contract that lets a single middleware artifact run either Bluetooth backend depending on which SDK provider the product image installed.

### Modified Capabilities
- `bluetooth-backend-selection`: Unchanged in behavior. This change makes its runtime check reachable on non-SDK products and preserves the truthfulness of the path it tests.

## Impact

- New stub repository and recipe providing `virtual/bluetooth-sdk` at middleware architecture.
- Real `bluetoothsdk` recipe: gains `PROVIDES`/`RPROVIDES` for the virtual, and a `SONAME` if it does not already declare one.
- `meta-middleware-*` machine config: default `PREFERRED_PROVIDER_virtual/bluetooth-sdk` set to the stub.
- Product machine config: override to the real `bluetoothsdk` on SDK-enabled products.
- `Bluetooth/CMakeLists.txt`: conditional backend selection and the `FATAL_ERROR` fallback are removed; both backends always compile, the SDK always links.
- `Bluetooth/BtAdapter.cpp`: unchanged. The runtime selection logic is correct as written.
- `meta-rdk/recipes-connectivity/bluetooth/files/btmgr-bluetooth-sdk.conf`: unchanged, and must remain so — its condition depends on the stub never occupying the real SDK path.
- Every image gains a runtime dependency on `libsdbus-c++`, because the stub must emit `bluetooth::Manager`'s constructor, vtable, and typeinfo, all of which reference its `sdbus::ProxyInterfaces` base.
- L1/L2 test builds are unaffected; they never link the real or stub SDK.

## Non-Goals

- Changing the runtime selection rule established by `runtime-backend-selection`.
- Making the stub functional. It is never called and its bodies are no-ops.
- Refactoring the Bluetooth SDK's public headers, which are owned by another team.
- Splitting the backends into separately packaged provider libraries. That was evaluated and set aside as a larger restructuring than a temporary bridge warrants.
- Changing the JSON-RPC surface or plugin API.
