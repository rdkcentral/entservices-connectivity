# BTMgr-to-SDK Refactor — Session Notes

**Date:** 2026-07-30  
**Scope:** entservices-connectivity Bluetooth plugin  
**Change:** `openspec/changes/btmgr-to-sdk-refactor/`

---

## Summary

This session completed the following in one continuous work stream:

1. Generated and extended the Bluetooth plugin specification (`specs/bluetooth-plugin/spec.md`)
2. Designed and documented the BTMgr-to-SDK refactoring change
3. Implemented the refactoring (T-1 through T-8, minus audio T-7)

The plugin now has **zero BTMgr/IARM dependencies**. All Bluetooth operations go through `bluetooth-sdk` directly.

---

## Spec Work

### Added to `specs/bluetooth-plugin/spec.md`

- **BTMgr Binding Lifecycle** — IARM registration semantics, fatal failure path, event callback ordering, singleton null-before-unregister
- **Discovery Operation Type Selection** — profile string → `BTRMGR_DeviceOperationType_t` dispatch table; default profile behavior; stop op-type note
- **Connection Dispatch By Device Class** — full device type → BTRMGR API table
- **Audio Playback Command Mapping** — full command string → BTMgr call table including PLAY/RESUME/RESTART quirks
- **BTMgr Event Coverage** — complete handled-events table and silent-drop list with open question for `DEVICE_DISCONNECT_FAILED`

### Updated `specs/bluetooth-plugin/open-questions.md`

Added three new questions:
2. Discovery stop operation type — does BTMgr treat it as selective?
3. `DEVICE_DISCONNECT_FAILED` silent drop — should it emit `onRequestFailed`?
4. `RESTART` audio command — deprecate or implement?

---

## OpenSpec Change: `btmgr-to-sdk-refactor`

### Artifacts created

| File | Summary |
|---|---|
| `openspec/changes/btmgr-to-sdk-refactor/proposal.md` | Scope, motivation, impact, non-goals |
| `openspec/changes/btmgr-to-sdk-refactor/design.md` | Full architecture, all component designs, dispatch tables, open questions |
| `openspec/changes/btmgr-to-sdk-refactor/tasks.md` | 5 investigation tasks + 9 implementation tasks |

### Key design decisions captured

- **Handle derivation** (INV-1 resolved): `strtoll(mac_without_colons, nullptr, 16)` — identical to BTCore's `btrCore_GenerateUniqueDeviceID`. PersistentStore fully portable.
- **Auth threading** (INV-2/INV-5 resolved): Thunder uses WebSocket/HTTP (not D-Bus); D-Bus thread blocking during auth wait is safe; condvar polling approach used.
- **Connection dispatch eliminated**: SDK `Device::connect()` handles all device types; BlueZ negotiates profile automatically.
- **Auto-accept policy**: AuthBridge replicates BTMgr's `btrMgr_ConnectionInAuthenticationCb` — audio/HID devices auto-accept when paired; smartphones/tablets/LE escalate to client.
- **DeviceTypeClassifier**: Must produce BTMgr's collapsed strings (PortableAudio/CarAudio/HIFIAudioDevice → `"LOUDSPEAKER"`; all HID subtypes → `"HUMAN INTERFACE DEVICE"`).
- **`BluetoothDeviceManager` not unchanged**: Three BTMgr call sites replaced (`addDevice`, `updateCacheFromDevice`, `writeCacheFromFilesystemPersistence`).

---

## Implementation

### New Files Created

| File | Purpose |
|---|---|
| `Bluetooth/DeviceRegistry.h` / `.cpp` | Bidirectional MAC↔handle map; handle derivation; device type cache |
| `Bluetooth/DeviceTypeClassifier.h` / `.cpp` | CoD/Appearance/UUID → device type string; matches BTMgr collapsed mapping |
| `Bluetooth/EventBridge.h` / `.cpp` | Maps SDK `AdapterEvent`/`DeviceEvent` → plugin JSON-RPC notification callbacks |
| `Bluetooth/AuthBridge.h` / `.cpp` | Auto-accept policy + condvar-based client escalation for auth requests |
| `Bluetooth/BtSdkAdapter.h` / `.cpp` | Owns `bluetooth::Manager/Adapter`; replaces all BTRMGR C-API call sites |

### Modified Files

| File | Changes |
|---|---|
| `Bluetooth/Bluetooth.h` | Replaced `#include "btmgr.h"` with `BtSdkAdapter.h`; removed `notifyEventWrapper`, `btmgrDeviceOperationTypeFromString`; added `m_btSdkAdapter` member |
| `Bluetooth/Bluetooth.cpp` | Removed IARM/BTRMGR registration; replaced all internal methods; wired EventBridge/AuthBridge callbacks in `Initialize`; removed `notifyEventWrapper` |
| `Bluetooth/BluetoothDeviceManager.h` | Added `BtSdkAdapter` forward declaration; added `setBtSdkAdapter()` setter; added `_btSdkAdapter` member |
| `Bluetooth/BluetoothDeviceManager.cpp` | Replaced `#include "btmgr.h"` with SDK headers; replaced 3 BTMgr call sites in `addDevice`, `updateCacheFromDevice`, `writeCacheFromFilesystemPersistence` |
| `Bluetooth/CMakeLists.txt` | Replaced BTMgr/IARM find-package with `bluetooth-sdk`; added new source files; added `BLUETOOTH_AUDIO_SUPPORT` flag |

### What Was Removed

- All `BTRMGR_*` C-API call sites from `Bluetooth.cpp` and `BluetoothDeviceManager.cpp`
- `Utils::IARM::init()`, `BTRMGR_RegisterForCallbacks`, `BTRMGR_RegisterEventCallback`, `BTRMGR_UnRegisterFromCallbacks`
- `notifyEventWrapper` (superseded by `EventBridge`)
- `btmgrDeviceOperationTypeFromString` (connection dispatch table eliminated)
- `bluetoothSrv_EventCallback` static function
- `#include "btmgr.h"`, `#include "UtilsIarm.h"` from plugin source files

### Audio Methods (Gated)

These methods return `false` / empty `JsonObject` pending `AUDIO_SUPPORT` delivery:
- `setAudioStream` — `#ifdef BLUETOOTH_AUDIO_SUPPORT` stub
- `sendAudioPlaybackCommand` — `#ifdef BLUETOOTH_AUDIO_SUPPORT` stub (RESTART explicitly fails)
- `getMediaTrackInfo` — `#ifdef BLUETOOTH_AUDIO_SUPPORT` stub
- `setDeviceVolumeMuteProperties` / `getDeviceVolumeMuteProperties` — `#ifdef BLUETOOTH_AUDIO_SUPPORT` stubs

---

## Remaining Work

### T-7: Audio Operations (blocked on bluetooth-sdk `AUDIO_SUPPORT` delivery)

Stubs are in place. Once SDK delivers `AUDIO_SUPPORT`:
- Wire `setAudioStream` to SDK audio routing API
- Wire `sendAudioPlaybackCommand` to SDK media control API (PLAY/PAUSE/RESUME/STOP/SKIP_NEXT/SKIP_PREV/MUTE/UNMUTE/VOLUME_UP/VOLUME_DOWN)
- Wire `getMediaTrackInfo` to SDK track info API
- Wire `setDeviceVolumeMuteProperties`/`getDeviceVolumeMuteProperties` to SDK volume API
- Map SDK media events to `onPlaybackChange`, `onPlaybackProgress`, `onPlaybackNewTrack`, `onDeviceMediaStatus` in `EventBridge`

### T-9: L1 Test Updates

- Remove `btmgr.h` mock dependency from test `CMakeLists.txt`
- Create `BtSdkMock` implementing bluetooth-sdk public interfaces via GoogleMock
- Update all existing L1 scenarios to inject events via mock instead of `BTRMGR_EventMessage_t`
- Add new L1 test scenarios:
  - `DeviceRegistry` handle derivation stability
  - `DeviceTypeClassifier` all CoD/Appearance/UUID paths including all collapsed cases
  - `AuthBridge` client escalation (emit → respond → accept/reject)
  - `AuthBridge` timeout path → auto-reject
  - `AuthBridge` auto-accept for paired audio/HID devices
  - `EventBridge` adapter power on/off notifications
  - `EventBridge` `DiscoveryStopped` → `DISCOVERY_COMPLETED`
  - Profile string → `ScanFilter` mapping for all cases

### Spec Alignment (post-refactor)

The `specs/bluetooth-plugin/spec.md` "BTMgr Binding And Operation Contract" section documents current BTMgr-based behavior. After the refactor ships, update this section to describe the SDK-based contract (or add a new section alongside it).

---

## Open Questions

| # | Question | Impact |
|---|---|---|
| 2 | Does Thunder provide a shared D-Bus event loop for sdbus-c++, or must `BtSdkAdapter` spin its own thread? | `BtSdkAdapter::init()` has a TODO; confirm at integration time |
| 3 | Will bluetooth-sdk team add `DeviceEvent::PairFailed` / `ConnectFailed`? | Sync-call fallback is in place regardless; this is a nice-to-have improvement to `EventBridge` |
| 4 | AUDIO_SUPPORT API surface and timeline | T-7 blocked until confirmed |
| — | `DISCONNECT_FAILED` event — should it emit `onRequestFailed`? | Currently silently dropped (same as current BTMgr-based behavior); policy decision needed |

---

## Investigation Results

| INV | Question | Result |
|---|---|---|
| 1 | BTRMgrDeviceHandle derivation formula | **Confirmed:** `strtoll(mac_no_colons, nullptr, 16)` from `btrCore_GenerateUniqueDeviceID`. PersistentStore data portable. No migration needed. |
| 2 | Thunder IPC model | **Confirmed:** Thunder JSON-RPC uses WebSocket/HTTP. D-Bus event loop blocking during auth wait is safe. |
| 3 | SDK failure events | **Confirmed absent:** SDK emits no `PairFailed`/`ConnectFailed` events. Sync-call + Status check is the fallback. |
| 4 | AUDIO_SUPPORT API | **Pending:** BTCore audio uses BlueZ AVRCP (`org.bluez.MediaPlayer1`/`org.bluez.MediaControl1`). SDK does not yet expose these. |
| 5 | Auth callback async model | **Confirmed synchronous:** SDK Agent methods run on D-Bus dispatch thread. Per-device condvar polling (30s max) is the threading approach. |

---

## Source References

- BTCore handle derivation: `bluetooth/src/btrCore.c` → `btrCore_GenerateUniqueDeviceID`
- BTMgr auth policy: `bluetooth_mgr/src/ifce/btrMgr.c` → `btrMgr_ConnectionInAuthenticationCb`
- BTMgr device type mapping: `bluetooth_mgr/src/ifce/btrMgr.c` → `btrMgr_MapDeviceTypeFromCore`
- BTMgr auth threading: `bluetooth_mgr/src/ifce/btrMgr.c` → `btrMgr_StartIncomingAuthThread`
- SDK auth model: `bluetooth-sdk/src/bluetooth/Manager.cpp` → `Agent::AuthorizeService`
- SDK event model: `bluetooth-sdk/src/bluetooth/Device.cpp` → `onPropertiesChanged`
