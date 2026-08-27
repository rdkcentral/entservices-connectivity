## Why

The Bluetooth plugin currently hard-codes its backend choice at build time via `BLUETOOTH_USE_SDK` in the adapter layer and `find_package(BLUETOOTH_SDK)` / `find_package(BTMGR)` in `Bluetooth/CMakeLists.txt`. That prevents the plugin from adapting to the host environment at startup and makes the runtime decision depend on the build image instead of the actual filesystem state.

The plugin needs a runtime-selected backend so it can prefer the bluetooth-sdk when the SDK library is present and otherwise fall back to the legacy BTMgr implementation. This preserves compatibility across mixed deployment images while keeping the runtime path deterministic and fixed for the lifetime of the plugin instance.

## What Changes

- Add a runtime backend-selection step during plugin initialization that checks whether `/usr/lib/bluetoothsdk/librdk_bluetooth.so` is present.
- Build both the SDK-backed and BTMgr-backed adapter implementations into the plugin binary so either implementation can be selected at runtime.
- Keep the backend choice fixed for the lifetime of the plugin instance once initialization has selected one implementation.
- Preserve the existing adapter abstraction and test injection model, while moving the production decision from compile-time CMake branching to runtime selection logic.
- Maintain the current public JSON-RPC and plugin interfaces without changing the external API contract.

## Capabilities

### New Capabilities
- `bluetooth-backend-selection`: Runtime decision logic that selects the active Bluetooth backend based on host environment and availability.

### Modified Capabilities
- `bluetooth-adapter`: The adapter dispatch layer is extended to support selecting a concrete implementation at runtime while keeping the same IBtAdapter contract.

## Impact

- `Bluetooth/BtAdapter.h` and `Bluetooth/BtAdapter.cpp`: production default implementation selection moves from compile-time macros to runtime instance selection.
- `Bluetooth/CMakeLists.txt`: build logic is updated to compile both backend implementations instead of choosing a single backend during configuration.
- `Bluetooth/IBtAdapter.h`: remains the stable abstraction boundary for both implementations.
- `Bluetooth/BtSdkAdapterImpl.*` and `Bluetooth/BtMgrAdapterImpl.*`: both remain valid, but the active instance is chosen at runtime rather than by CMake.
- Existing test overrides via `BtAdapter::setImpl()` remain supported and continue to take precedence in L1/L2 test builds.
