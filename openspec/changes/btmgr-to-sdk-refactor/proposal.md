## Why

The Bluetooth plugin currently depends on BTMgr/BTCore via the BTRMGR C API and IARM for all Bluetooth operations. The RDK-E Bluetooth re-architecture (Phase 4) removes BTManager entirely and replaces it with the bluetooth-sdk, an object-oriented C++ library that communicates directly with BlueZ over D-Bus. This change refactors the entservices-connectivity Bluetooth Thunder plugin to use the bluetooth-sdk instead of BTRMGR, eliminating the IARM and BTMgr runtime dependencies.

A second requirement was added after initial implementation: the plugin must be buildable against **either** the bluetooth-sdk (when available) **or** the legacy BTMgr (as a fallback), determined at CMake configure time via `find_package`. Build image footprint must be minimised — SDK builds include no BTMgr object code or headers; BTMgr builds include no SDK object code or headers.

## What Changes

- Remove all `BTRMGR_*` C API calls from `Bluetooth.cpp` and replace with calls through an internal adapter interface.
- Remove IARM registration and event callback lifecycle from `Bluetooth.cpp`; move to the SDK-path adapter implementation.
- Introduce `IBtAdapter` (renamed from the initial `IBtSdkAdapter`) as the backend-neutral pure virtual interface. Both the SDK-path and BTMgr-path implementations satisfy this interface.
- Rename the dispatch wrapper class to `BtAdapter` (was `BtSdkAdapter`) and update all associated file names to remove "Sdk" from backend-neutral artifacts.
- Rename `BtSdkAdapterRealImpl` to `BtSdkAdapterImpl` (removing the redundant "Real").
- Introduce `BtSdkAdapterImpl` — the bluetooth-sdk implementation of `IBtAdapter`, wrapping `bluetooth::Manager/Adapter/Device`. Includes `DeviceRegistry`, `DeviceTypeClassifier`, `EventBridge`, `AuthBridge`.
- Introduce `BtMgrAdapterImpl` — the BTMgr implementation of `IBtAdapter`, extracting and wrapping all `BTRMGR_*` call sites and IARM lifecycle from the old `Bluetooth.cpp`.
- Extend `IBtAdapter` to include audio operations (`setAudioStream`, `setAudioControlCommand`, `setDeviceVolumeMute`, `getDeviceVolumeMute`, `getMediaTrackInfo`). `BtSdkAdapterImpl` provides stubs pending AUDIO_SUPPORT (T-7). `BtMgrAdapterImpl` provides working BTMgr implementations immediately.
- `CMakeLists.txt`: `find_package(BluetoothSDK)` / `find_package(BTMGR)` conditional; compiles the correct impl, links the correct library.
- `Bluetooth.cpp` audio method bodies become single-line delegates to `m_btAdapter`; all `#ifdef BLUETOOTH_AUDIO_SUPPORT` stubs removed from `Bluetooth.cpp`.

## Capabilities

### New Capabilities
- `bluetooth-adapter`: Internal adapter layer (not API-facing). Covers `IBtAdapter`, `BtAdapter` dispatch wrapper, `BtSdkAdapterImpl` (SDK path), `BtMgrAdapterImpl` (BTMgr path), `DeviceRegistry`, `DeviceTypeClassifier`, `EventBridge`, `AuthBridge`.

### Modified Capabilities
- `bluetooth-btmgr-binding`: Replaced end-to-end. All requirements in the BTMgr Binding And Operation Contract spec section are superseded by SDK-based equivalents when the SDK path is active.

## Impact

- `Bluetooth/Bluetooth.cpp` and `Bluetooth/Bluetooth.h`: `BtSdkAdapter m_btSdkAdapter` renamed to `BtAdapter m_btAdapter`; include updated. All `m_btSdkAdapter.*` call sites renamed. Audio method bodies replaced with single-line adapter delegates.
- File renames: `IBtSdkAdapter.h` → `IBtAdapter.h`, `BtSdkAdapterCallbacks.h` → `BtAdapterCallbacks.h`, `BtSdkAdapter.h/.cpp` → `BtAdapter.h/.cpp`, `BtSdkAdapterRealImpl.h/.cpp` → `BtSdkAdapterImpl.h/.cpp`, `BtSdkAdapterMock.h` → `BtAdapterMock.h`.
- Class renames: `IBtSdkAdapter` → `IBtAdapter`, `BtSdkAdapter` → `BtAdapter`, `BtSdkAdapterRealImpl` → `BtSdkAdapterImpl`, `BtSdkAdapterImplMock` → `BtAdapterImplMock`.
- New files: `BtMgrAdapterImpl.h`, `BtMgrAdapterImpl.cpp`.
- `BluetoothDeviceManager` (`.h` and `.cpp`): `setBtSdkAdapter` → `setBtAdapter`; `IBtSdkAdapter*` → `IBtAdapter*`.
- `CMakeLists.txt`: SDK/BTMgr conditional source and link selection.
- JSON-RPC API surface: No changes.
- PersistentStore data: Device handle values remain stable.
- L1 tests: Mock renamed `BtAdapterMock.h` / `BtAdapterImplMock`; audio mock methods added.

## Non-Goals

- Changing the JSON-RPC or COM-RPC public API
- Audio routing via PipeWire (this is Phase 3; assumed completed before this change lands)
- Implementing bluetooth-sdk's AUDIO_SUPPORT module (SDK team responsibility)
- GATT server functionality
- Multi-adapter support
