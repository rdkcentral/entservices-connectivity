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

### ~~T-9a: Refactor BtSdkAdapter to static-impl dispatch pattern~~ ✓ DONE\n### ~~T-9b: Add mock files to entservices-testframework~~ ✓ DONE\n### ~~T-9c: Update test_Bluetooth.cpp~~ ✓ DONE\n\nNew files: `IBtSdkAdapter.h`, `BtSdkAdapterCallbacks.h`, `BtSdkAdapterRealImpl.h/.cpp`,\n`Tests/mocks/BtSdkAdapterMock.h`. `BtSdkAdapter` now implements `IBtSdkAdapter` via static-impl dispatch.

Restructure `BtSdkAdapter` in the plugin to follow the same static-impl dispatch pattern used
by the existing `Btmgr` mock infrastructure, so testframework can shadow it with a mock.

Changes in `entservices-connectivity/Bluetooth/`:
- Extract `IBtSdkAdapter` pure virtual interface from `BtSdkAdapter.h` (covers all public methods
  currently on `BtSdkAdapter`: `init`, `deinit`, `startScan`, `stopScan`, `getAdapterPowered`,
  `setAdapterPowered`, `getAdapterName`, `setAdapterName`, `isAdapterDiscoverable`,
  `setAdapterDiscoverable`, `getPairedDevices`, `getConnectedDevices`, `getDiscoveredDevices`,
  `pairDevice`, `unpairDevice`, `connectDevice`, `disconnectDevice`, `getDeviceProperties`,
  `getDeviceByHandle`, `respondToEvent`)
- Rename current `BtSdkAdapter` class to `BtSdkAdapterRealImpl : public IBtSdkAdapter`
  in a new `BtSdkAdapterRealImpl.cpp` (keeps all `bluetooth::Manager/Adapter/Device` usage)
- `BtSdkAdapter.h` (kept in plugin) becomes the thin dispatch wrapper:
  ```
  class BtSdkAdapter {
      static IBtSdkAdapter* impl;
      static void setImpl(IBtSdkAdapter* newImpl);
      // thin dispatch methods delegating to impl->X()
      std::string init(...); bool startScan(...); etc.
      static std::string handleForMac(const std::string& mac); // kept static, no impl needed
  };
  ```
- `BtSdkAdapter.cpp` (kept in plugin) becomes the dispatch implementation + default
  construction: on first call to `init()` if `impl == nullptr`, constructs and assigns
  `BtSdkAdapterRealImpl` automatically (so production code path is unchanged)
- `Bluetooth.h` holds `BtSdkAdapter m_btSdkAdapter` unchanged (value member, no pointer change)
- In test builds, testframework provides its own `BtSdkAdapter.h` (mock version) via
  include path ordering, shadowing the plugin's version (same mechanism as `btmgr.h`)

Depends on: (none — standalone production change)

### T-9b: Add mock files to entservices-testframework

Add three files to `entservices-testframework/Tests/mocks/`:

**`BtSdkAdapter.h`** (mock version — shadows plugin's `BtSdkAdapter.h` in test builds):
- Defines `IBtSdkAdapter` pure virtual interface (same definition as plugin)
- Defines `BtSdkAdapter` thin dispatch class with `static IBtSdkAdapter* impl` and `setImpl()`
- Does NOT include any bluetooth-sdk headers

**`BtSdkAdapter.cpp`** (dispatch + setImpl — replaces real impl in test builds):
- Implements `BtSdkAdapter::setImpl()` and all dispatch methods
- Does NOT construct `BtSdkAdapterRealImpl` (no SDK in test build)

**`BtSdkAdapterMock.h`** (GoogleMock implementation):
```cpp
class BtSdkAdapterImplMock : public IBtSdkAdapter {
public:
    MOCK_METHOD(std::string, init, (...), (override));
    MOCK_METHOD(bool, startScan, (const std::string&), (override));
    MOCK_METHOD(bool, stopScan, (), (override));
    MOCK_METHOD(bool, getAdapterPowered, (bool&), (const, override));
    MOCK_METHOD(bool, setAdapterPowered, (bool), (override));
    // ... all methods from IBtSdkAdapter

    // Event injection helpers (no equivalent in BTMgr mock — new capability):
    std::function<void(AdapterEvent, AdapterEventData)> m_adapterEventCb;
    std::function<void(DeviceEvent, std::shared_ptr<bluetooth::Device>)> m_deviceEventCb;

    // Called by tests to simulate SDK events reaching the plugin:
    void fireAdapterEvent(AdapterEvent e, AdapterEventData d) {
        if (m_adapterEventCb) m_adapterEventCb(e, d);
    }
    void fireDeviceEvent(DeviceEvent e, std::shared_ptr<bluetooth::Device> dev) {
        if (m_deviceEventCb) m_deviceEventCb(e, dev);
    }
};
```

Add `BtSdkAdapter.cpp` to `TestMocklib` sources in testframework `CMakeLists.txt`.

Depends on: T-9a

### T-9c: Update test_Bluetooth.cpp

**Fixture changes:**
- Remove `BtmgrImplMock* p_btmgrMock` and all `Btmgr::setImpl()` / cleanup
- Remove `IarmBusImplMock* p_iarmBusImplMock` and all `IarmBus::setImpl()` / cleanup
- Remove `#include "btmgrMock.h"` and `#include "IarmBusMock.h"`
- Add `BtSdkAdapterImplMock* p_btSdkMock = new NiceMock<BtSdkAdapterImplMock>`
- Add `BtSdkAdapter::setImpl(p_btSdkMock)` in ctor; `BtSdkAdapter::setImpl(nullptr)` + `delete` in dtor
- Default `ON_CALL` on `init()` returns `""` (success)

**Port existing tests** (1-for-1 coverage of all current test scenarios):
- `startScan_*` → `EXPECT_CALL(*p_btSdkMock, startScan(profile))` instead of `BTRMGR_StartDeviceDiscovery`
- `stopScan_*` → `EXPECT_CALL(*p_btSdkMock, stopScan())`
- `isDiscoverable_*` → `EXPECT_CALL(*p_btSdkMock, isAdapterDiscoverable(::testing::_))`
- `connect_*` → `EXPECT_CALL(*p_btSdkMock, connectDevice(handleStr))`
- `pair_*` → `EXPECT_CALL(*p_btSdkMock, pairDevice(handleStr))`
- `getDiscoveredDevices_*` → `EXPECT_CALL(*p_btSdkMock, getDiscoveredDevices())`
- `getPairedDevices_*` → `EXPECT_CALL(*p_btSdkMock, getPairedDevices())`
- `getConnectedDevices_*` → `EXPECT_CALL(*p_btSdkMock, getConnectedDevices())`
- etc. for all remaining methods

**New test scenarios** (capabilities not in current tests):

*DeviceRegistry unit tests:*
- Handle derivation: `"AA:BB:CC:DD:EE:FF"` → `"187723572702975"` (decimal of `0xAABBCCDDEEFF`)
- Round-trip: register → lookup by handle → lookup by MAC → unregister → nullptr
- Type cache: set/get/overwrite

*DeviceTypeClassifier unit tests:*
- BLE appearance `0x03c0` → `"HUMAN INTERFACE DEVICE"`
- BLE appearance `0x0040` → `"SMARTPHONE"`
- BLE appearance `0x0200` → `"LE TILE"`
- CoD Audio/Video minor 0x06 → `"HEADPHONES"`
- CoD Audio/Video minor 0x05 → `"LOUDSPEAKER"`
- CoD Audio/Video minor 0x07 (PortableAudio) → `"LOUDSPEAKER"` ← collapsed, not "PORTABLE AUDIO"
- CoD Audio/Video minor 0x0b (HiFi) → `"LOUDSPEAKER"` ← collapsed
- CoD Peripheral → `"HUMAN INTERFACE DEVICE"`
- UUID fallback: AudioSink → `"HEADPHONES"`, AudioSource → `"SMARTPHONE"`

*EventBridge tests (via event injection):*
- `DiscoveryStarted` event → `onStatusChanged(DISCOVERY_STARTED)` notification emitted
- `DiscoveryStopped` event → `onStatusChanged(DISCOVERY_COMPLETED)` notification emitted
- `PoweredOn` event → `onStatusChanged(HARDWARE_AVAILABLE)` notification emitted
- `DeviceEvent::Paired` → `onStatusChanged(PAIRING_CHANGE, paired=true)`
- `DeviceEvent::Connected` → `onStatusChanged(CONNECTION_CHANGE, connected=true)`
- `DeviceEvent::Disconnected` → `onStatusChanged(CONNECTION_CHANGE, connected=false)`

*AuthBridge tests:*
- `onConnectionRequest` for paired HEADPHONES device → no notification, auto-accepted
- `onConnectionRequest` for paired HID device → no notification, auto-accepted
- `onConnectionRequest` for SMARTPHONE → `onConnectionRequest` notification emitted
- Client calls `respondToEvent(ACCEPTED)` → auth resolves true
- Client calls `respondToEvent(REJECTED)` → auth resolves false
- No client response within timeout → auto-rejects

Update `entservices-connectivity/Tests/L1Tests/CMakeLists.txt`:
- Remove `btmgr`-related includes and links
- Add `bluetooth-sdk` stub includes if needed for `IBtSdkAdapter` types
- No link change to `TestMocklib` needed (BTMgr dispatch is just removed)

Depends on: T-9a, T-9b

---

---

## Dual-Backend + Rename Tasks

### ~~T-10: Rename files and classes — BTMgr-neutral naming~~ ✓ DONE

Mechanical rename of all `BtSdk`-prefixed adapter artifacts to backend-neutral names. No logic changes.

**File renames (plugin — `entservices-connectivity/Bluetooth/`):**
- `IBtSdkAdapter.h` → `IBtAdapter.h`
- `BtSdkAdapterCallbacks.h` → `BtAdapterCallbacks.h`
- `BtSdkAdapter.h` → `BtAdapter.h`
- `BtSdkAdapter.cpp` → `BtAdapter.cpp`
- `BtSdkAdapterRealImpl.h` → `BtSdkAdapterImpl.h`
- `BtSdkAdapterRealImpl.cpp` → `BtSdkAdapterImpl.cpp`

**File renames (testframework — `entservices-testframework/Tests/mocks/`):**
- `BtSdkAdapterMock.h` → `BtAdapterMock.h`
- Testframework's `BtSdkAdapter.h` → `BtAdapter.h`
- Testframework's `BtSdkAdapter.cpp` → `BtAdapter.cpp`

**Class renames (all occurrences across all files):**
- `IBtSdkAdapter` → `IBtAdapter`
- `BtSdkAdapter` → `BtAdapter`
- `BtSdkAdapterRealImpl` → `BtSdkAdapterImpl`
- `BtSdkAdapterImplMock` → `BtAdapterImplMock`

**Member and method renames in `Bluetooth.h/.cpp` and `BluetoothDeviceManager.h/.cpp`:**
- `m_btSdkAdapter` → `m_btAdapter`
- `setBtSdkAdapter(...)` → `setBtAdapter(...)`
- `IBtSdkAdapter::BtDeviceProperties` → `IBtAdapter::BtDeviceProperties`
- `IBtSdkAdapter::BtDeviceInfo` → `IBtAdapter::BtDeviceInfo`

**`CMakeLists.txt` source name updates:**
- `BtSdkAdapter.cpp` → `BtAdapter.cpp`
- `BtSdkAdapterRealImpl.cpp` → `BtSdkAdapterImpl.cpp`

**`BtAdapter.cpp` internal rename:**
- `g_realImpl` → `g_btAdapterImpl`

Depends on: T-9a (T-9a must be complete so the files exist to rename)

### ~~T-11: Add audio methods to IBtAdapter + stubs in BtSdkAdapterImpl~~ ✓ DONE

Extend the interface and SDK-path stub so audio operations route through the adapter like all other operations, eliminating `#ifdef BLUETOOTH_AUDIO_SUPPORT` from `Bluetooth.cpp`.

**`IBtAdapter.h` additions:**
```cpp
struct BtMediaTrackInfo {
    std::string album, genre, title, artist;
    uint32_t    duration{0}, trackNumber{0}, numberOfTracks{0};
};
struct BtDeviceVolumeMute {
    uint8_t volume{0};
    bool    mute{false};
    bool    valid{false};
};
virtual bool               setAudioStream(long long int deviceID,
                                           const std::string& streamName) = 0;
virtual bool               setAudioControlCommand(long long int deviceID,
                                                   const std::string& cmd) = 0;
virtual bool               setDeviceVolumeMute(long long int deviceID,
                                                const std::string& profile,
                                                uint8_t volume, bool mute) = 0;
virtual BtDeviceVolumeMute getDeviceVolumeMute(long long int deviceID,
                                               const std::string& profile) const = 0;
virtual BtMediaTrackInfo   getMediaTrackInfo(long long int deviceID) const = 0;
```

**`BtAdapter.h/.cpp`**: add 5 dispatch methods (same one-liner pattern as existing methods).

**`BtSdkAdapterImpl.h/.cpp`**: add 5 override declarations + stub implementations returning `false` / empty struct, gated by `#ifdef BLUETOOTH_AUDIO_SUPPORT` pending T-7.

**`Bluetooth.cpp`**: replace the 5 audio method bodies (currently `#ifdef BLUETOOTH_AUDIO_SUPPORT` stubs) with single-line delegates to `m_btAdapter`. Remove all `#ifdef BLUETOOTH_AUDIO_SUPPORT` from `Bluetooth.cpp`. `Bluetooth.cpp` no longer needs the `BLUETOOTH_AUDIO_SUPPORT` define at all.

**`BtAdapterMock.h`**: add 5 `MOCK_METHOD` entries for the new audio methods.

Depends on: T-10

### ~~T-12: Create BtMgrAdapterImpl (BTMgr fallback backend)~~ ✓ DONE

New file pair implementing `IBtAdapter` using BTRMGR calls. All logic is extracted from `entservices-connectivity.develop/Bluetooth/Bluetooth.cpp` and adapted to the interface.

**`BtMgrAdapterImpl.h`**: declare `class BtMgrAdapterImpl : public IBtAdapter` with all override declarations.

**`BtMgrAdapterImpl.cpp`**: implement all `IBtAdapter` methods:

*Lifecycle:*
- `init()`: `BTRMGR_RegisterForCallbacks()` + `BTRMGR_RegisterEventCallback(staticEventCb)`. The static callback maps `BTRMGR_EventMessage_t` to `BtEventCallbacks` (same events as `EventBridge` produces for SDK path).
- `deinit()`: `BTRMGR_UnRegisterFromCallbacks()`

*Adapter operations:* `getAdapterPowered`, `setAdapterPowered`, `getAdapterName`, `setAdapterName`, `isAdapterDiscoverable`, `setAdapterDiscoverable` — extracted from BTMgr `Bluetooth.cpp`.

*Discovery:* `startScan` → `BTRMGR_StartDeviceDiscovery`; `stopScan` → `BTRMGR_StopDeviceDiscovery`.

*Device lists:* `getDiscoveredDevices`, `getPairedDevices`, `getConnectedDevices` → `BTRMGR_GetDiscoveredDevices`, `BTRMGR_GetPairedDevices`, `BTRMGR_GetConnectedDevices`. Build `BtDeviceInfo` structs from BTRMGR device property structs.

*Device operations:* `pairDevice`, `unpairDevice`, `connectDevice`, `disconnectDevice`, `getDeviceProperties`, `getMacForHandle`, `respondToEvent`.

*Handle derivation (inline):* `strtoll(mac_no_colons, NULL, 16)` — no `DeviceRegistry` dependency.

*Audio operations:* all 5 methods with working BTRMGR implementations (extracted from legacy `Bluetooth.cpp`).

Depends on: T-11

### ~~T-13: CMakeLists dual-backend selection~~ ✓ DONE

Update `entservices-connectivity/Bluetooth/CMakeLists.txt` to:
1. Probe for both backends: `find_package(BluetoothSDK QUIET)` and `find_package(BTMGR QUIET)`
2. If `BluetoothSDK_FOUND`: define `BLUETOOTH_USE_SDK=1`, add `BtSdkAdapterImpl.cpp` + bridge files, link `bluetooth-sdk`
3. Else if `BTMGR_FOUND`: add `BtMgrAdapterImpl.cpp`, add BTRMGR include dirs, link `${BTMGR_LIBRARIES}` + `${IARMBUS_LIBRARIES}`
4. Else: `message(FATAL_ERROR "Neither BluetoothSDK nor BTMGR found")`
5. Both paths compile: `Bluetooth.cpp`, `BluetoothDeviceManager.cpp`, `BtAdapter.cpp`, `DeviceRegistry.cpp` (SDK only — move to SDK block), `Module.cpp`

Depends on: T-12

### ~~T-14: Update tests and mocks for new names + audio~~ ✓ DONE

**`entservices-testframework/Tests/mocks/BtAdapter.h` (renamed from `BtSdkAdapter.h`):**
- Update all class names per T-10 rename.

**`entservices-testframework/Tests/mocks/BtAdapter.cpp` (renamed from `BtSdkAdapter.cpp`):**
- Update all class names per T-10 rename.

**`entservices-testframework/Tests/mocks/BtAdapterMock.h` (renamed from `BtSdkAdapterMock.h`):**
- Update class names.
- Add 5 `MOCK_METHOD` entries for audio methods (from T-11).

**`entservices-connectivity/Tests/mocks/BtAdapterMock.h` (renamed):**
- Same updates as testframework mock.

**`entservices-connectivity/Tests/L1Tests/tests/test_Bluetooth.cpp`:**
- `BtAdapterImplMock` (was `BtSdkAdapterImplMock`)
- `BtAdapter::setImpl(...)` (was `BtSdkAdapter::setImpl(...)`)
- All `IBtAdapter::BtDeviceInfo` / `IBtAdapter::BtDeviceProperties` references updated.

**`entservices-connectivity/Tests/L1Tests/CMakeLists.txt`:**
- Update file name references for renamed mock files.

Depends on: T-10, T-11

---

## Completion Criteria

- All `BTRMGR_*` includes and call sites removed from `Bluetooth.cpp` ✓ (routed through adapter)
- Plugin compiles without `btmgr.h` or IARM headers in SDK builds ✓
- Plugin compiles without any bluetooth-sdk headers in BTMgr builds
- `find_package(BluetoothSDK)` → SDK build; `find_package(BTMGR)` → BTMgr build; neither → fatal error
- All existing L1 tests pass with renamed mock
- New L1 tests cover all EventBridge, AuthBridge, DeviceTypeClassifier, and DeviceRegistry paths
- Audio methods delegate through `IBtAdapter` in both builds; no `#ifdef` in `Bluetooth.cpp`
- JSON-RPC API surface unchanged (method names, param shapes, event payloads)
- PersistentStore compatibility confirmed ✓ (handle formula verified, no migration needed)
- INV-1 through INV-4 resolved; design.md updated with confirmed answers
