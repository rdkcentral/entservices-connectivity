## Why

The Bluetooth plugin currently depends on BTMgr/BTCore via the BTRMGR C API and IARM for all Bluetooth operations. The RDK-E Bluetooth re-architecture (Phase 4) removes BTManager entirely and replaces it with the bluetooth-sdk, an object-oriented C++ library that communicates directly with BlueZ over D-Bus. This change refactors the entservices-connectivity Bluetooth Thunder plugin to use the bluetooth-sdk instead of BTRMGR, eliminating the IARM and BTMgr runtime dependencies.

## What Changes

- Remove all `BTRMGR_*` C API calls and replace with bluetooth-sdk equivalents.
- Remove IARM registration and event callback lifecycle; replace with sdk Manager and per-object event registration.
- Introduce `BtSdkAdapter` as an internal adapter layer that translates bluetooth-sdk events and operations into the existing plugin semantics, preserving the JSON-RPC API surface unchanged.
- Introduce `DeviceRegistry` to maintain stable numeric device handle identity backward compatible with existing clients and PersistentStore data.
- Introduce `DeviceTypeClassifier` to infer the device type strings (HEADPHONES, KEYBOARD, etc.) that the plugin currently derives from BTRMGR.
- Introduce `EventBridge` to map SDK per-object events to the plugin's existing JSON-RPC notification contracts.
- Introduce `AuthBridge` to bridge the SDK's synchronous auth callback to the plugin's existing async client respondToEvent model.
- Audio methods (`sendAudioPlaybackCommand`, `setAudioStream`, `setDeviceVolumeMuteInfo`, `getDeviceVolumeMuteInfo`, `getAudioInfo`) are re-routed via the SDK's `AUDIO_SUPPORT` module once it is implemented.

## Capabilities

### New Capabilities
- `bluetooth-sdk-adapter`: Internal adapter layer (not API-facing). Covers DeviceRegistry, DeviceTypeClassifier, EventBridge, AuthBridge, and all BTRMGR-to-SDK translation.

### Modified Capabilities
- `bluetooth-btmgr-binding`: Replaced end-to-end. All requirements in the BTMgr Binding And Operation Contract spec section are superseded by SDK-based equivalents.

## Impact

- `Bluetooth/Bluetooth.cpp` and `Bluetooth/Bluetooth.h`: BTRMGR includes and all `BTRMGR_*` call sites replaced. Initialization and deinitialization lifecycle replaced.
- New internal classes: `BtSdkAdapter`, `DeviceRegistry`, `DeviceTypeClassifier`, `EventBridge`, `AuthBridge`.
- `BluetoothDeviceManager` (`.h` and `.cpp`): public interface **unchanged**, but three internal BTMgr call sites replaced: `addDevice()`, `updateCacheFromDevice()`, and `writeCacheFromFilesystemPersistence()` (migration path). These are updated to use SDK device properties and device lists via `BtSdkAdapter`.
- `CMakeLists.txt`: Remove btmgr link target; add bluetooth-sdk link target.
- JSON-RPC API surface: No changes. All method names, parameter shapes, and event payloads are preserved.
- PersistentStore data: Device handle values stored in PersistentStore remain stable (DeviceRegistry ensures handle derivation is deterministic from MAC address).
- L1 tests: BTMgr mock replaced with bluetooth-sdk mock. Test coverage targets unchanged.

## Non-Goals

- Changing the JSON-RPC or COM-RPC public API
- Audio routing via PipeWire (this is Phase 3; assumed completed before this change lands)
- Implementing bluetooth-sdk's AUDIO_SUPPORT module (SDK team responsibility)
- GATT server functionality
- Multi-adapter support
