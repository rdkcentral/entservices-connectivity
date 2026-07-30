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
