# Tasks: BTMgr-to-SDK Refactor

## Investigation Tasks (must complete first)

- [x] **INV-1: BTRMgrDeviceHandle derivation scheme** ~~CLOSED~~
  Confirmed from `bluetooth/src/btrCore.c` (`btrCore_GenerateUniqueDeviceID`): strip colons from MAC, parse as base-16 integer via `strtoll`. PersistentStore data is fully portable — no handle migration needed.

- [x] **INV-2: D-Bus event loop and Thunder IPC model** ~~CLOSED~~
  Thunder JSON-RPC (`respondToEvent`) uses Thunder's own WebSocket/HTTP transport, independent of D-Bus. Blocking the D-Bus thread during auth wait is safe. AuthBridge uses simple condvar polling (30s max). BtSdkAdapter needs a dedicated event loop thread (confirm whether Thunder already provides one at integration time).

- [ ] **INV-3: SDK failure events** (nice-to-have, not blocking)
  Confirmed absent today — SDK emits no failure DeviceEvents. Plugin uses sync calls + Status check as the confirmed fallback. If SDK team adds `DeviceEvent::PairFailed` / `ConnectFailed` in future, EventBridge can be enhanced to use them directly.
  Owner: TBD. Does not block implementation.

- [ ] **INV-4: AUDIO_SUPPORT API surface**
  Obtain AUDIO_SUPPORT API specification from bluetooth-sdk team. Confirm method signatures for media control, track info, and volume operations. Note: audio uses BlueZ AVRCP (org.bluez.MediaPlayer1/MediaControl1) — confirmed from `btrCore_avMedia.c`.
  Owner: TBD. Blocks: T-7.

---

## Implementation Tasks

### ~~T-1: BtSdkAdapter skeleton and lifecycle~~ ✓ DONE
### ~~T-2: DeviceRegistry~~ ✓ DONE
### ~~T-3: DeviceTypeClassifier~~ ✓ DONE
### ~~T-4: EventBridge~~ ✓ DONE
### ~~T-5: AuthBridge~~ ✓ DONE
### ~~T-5a: BluetoothDeviceManager BTMgr call site replacements~~ ✓ DONE
### ~~T-6: Replace all adapter and device operation calls~~ ✓ DONE
### ~~T-8: CMakeLists.txt~~ ✓ DONE

New files created: `DeviceRegistry.h/.cpp`, `DeviceTypeClassifier.h/.cpp`, `EventBridge.h/.cpp`, `AuthBridge.h/.cpp`, `BtSdkAdapter.h/.cpp`

Remaining:

### T-7: Audio operations (AUDIO_SUPPORT)
- Gate audio method implementations with `BLUETOOTH_AUDIO_SUPPORT` compile flag (stub in place)
- Replace `setAudioStream` → SDK audio routing API (API from INV-4)
- Replace `sendAudioPlaybackCommand` → SDK media control (API from INV-4)
- Replace `getAudioInfo` (getMediaTrackInfo) → SDK track info (API from INV-4)
- Replace `setDeviceVolumeMuteProperties` / `getDeviceVolumeMuteProperties` → SDK volume APIs (API from INV-4)
- Map media SDK events (track started/paused/stopped/changed/position, device media status) in EventBridge
- Depends on: INV-4, T-4

### T-9: L1 test updates
- Remove `btmgr.h` mock dependency from test CMakeLists
- Create `BtSdkMock` implementing bluetooth-sdk public interfaces via GoogleMock
- Update all existing L1 test scenarios to inject events via `BtSdkMock` instead of `BTRMGR_EventMessage_t`
- Add L1 test scenarios for:
  - DeviceRegistry handle derivation stability
  - DeviceTypeClassifier all classification paths
  - AuthBridge: emit notification, client responds, verify accept/reject
  - AuthBridge: timeout path → auto-reject
  - EventBridge: adapter power on/off notifications
  - EventBridge: DiscoveryStopped → DISCOVERY_COMPLETED notification
  - Profile string → ScanFilter mapping for all profile types
- Depends on: T-1 through T-6

---

## Completion Criteria

- All `BTRMGR_*` includes and call sites removed from `Bluetooth/` ✓
- Plugin compiles without `btmgr.h` or IARM headers ✓ (pending full build verification)
- All existing L1 tests pass with new mock (T-9 pending)
- New L1 tests cover all EventBridge, AuthBridge, DeviceTypeClassifier, and DeviceRegistry paths (T-9 pending)
- JSON-RPC API surface unchanged ✓
- PersistentStore compatibility confirmed ✓ (handle formula verified)
- INV-1 through INV-5 resolved ✓ (INV-3 and INV-4 non-blocking)
- All existing L1 tests pass with new mock
- New L1 tests cover all EventBridge, AuthBridge, DeviceTypeClassifier, and DeviceRegistry paths
- JSON-RPC API surface unchanged (method names, param shapes, event payloads)
- PersistentStore compatibility confirmed (handle values stable or migration implemented)
- INV-1 through INV-4 resolved and design.md updated with confirmed answers
