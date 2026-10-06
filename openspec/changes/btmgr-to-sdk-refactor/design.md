# Design: BTMgr-to-SDK Refactor

## Architecture Overview

```
┌──────────────────────────────────────────────────────────────────────┐
│  Thunder Bluetooth Plugin                                            │
│                                                                      │
│  ┌────────────────────────────────────────────────────────────┐      │
│  │  Bluetooth (JSON-RPC surface — UNCHANGED)                  │      │
│  │  All method wrappers, event notifications, power policy    │      │
│  └──────────────────────────────┬─────────────────────────────┘      │
│                   m_btAdapter   │ BtAdapter (dispatch wrapper)       │
│  ┌──────────────────────────────▼─────────────────────────────┐      │
│  │  BtAdapter : IBtAdapter  (SDK-free dispatch wrapper)        │      │
│  │  setImpl() for test injection; g_btAdapterImpl for prod     │      │
│  └────────────┬───────────────────────────────┬───────────────┘      │
│               │ BLUETOOTH_USE_SDK             │ (no flag)            │
│  ┌────────────▼────────────┐   ┌──────────────▼──────────────┐      │
│  │  BtSdkAdapterImpl       │   │  BtMgrAdapterImpl           │      │
│  │  (SDK path)             │   │  (BTMgr fallback path)      │      │
│  │  DeviceRegistry         │   │  BTRMGR_* calls             │      │
│  │  EventBridge            │   │  IARM lifecycle             │      │
│  │  AuthBridge             │   │  inline handle derivation   │      │
│  │  DeviceTypeClassifier   │   └──────────────┬──────────────┘      │
│  └────────────┬────────────┘                  │                      │
│               │ bluetooth-sdk API             │ BTRMGR C API         │
│  ┌────────────▼────────────┐   ┌──────────────▼──────────────┐      │
│  │  bluetooth-sdk          │   │  BTMgr / IARM               │      │
│  │  Manager→Adapter→Device │   │  (legacy runtime)           │      │
│  └────────────┬────────────┘   └─────────────────────────────┘      │
│               │ sdbus-c++ / D-Bus                                    │
└───────────────┼──────────────────────────────────────────────────────┘
                │
             BlueZ
```

`BluetoothDeviceManager` owns PersistentStore access for paired device metadata. Its public interface is unchanged, but **three internal BTMgr call sites require SDK replacements in the SDK path** — see the BluetoothDeviceManager section below. In the BTMgr path these call sites remain BTRMGR calls inside `BtMgrAdapterImpl`.

---

## Component: BtAdapter / IBtAdapter

`IBtAdapter` (`IBtAdapter.h`) is the backend-neutral pure virtual interface. It is WPEFramework-free and SDK-free so it can be included in test builds and in both production backends without pulling in external library headers.

`BtAdapter` (`BtAdapter.h/.cpp`) is the thin static-impl dispatch wrapper. `Bluetooth.h` holds `BtAdapter m_btAdapter` as a value member. `BtAdapter::setImpl()` enables test mock injection. In production builds `BtAdapter.cpp` constructs a `static BtSdkAdapterImpl g_btAdapterImpl` (SDK path) or `static BtMgrAdapterImpl g_btAdapterImpl` (BTMgr path) as the default, selected by `#ifdef BLUETOOTH_USE_SDK`.

```cpp
// BtAdapter.cpp (production — only the impl include and static differ)
#ifdef BLUETOOTH_USE_SDK
    #include "BtSdkAdapterImpl.h"
    static BtSdkAdapterImpl g_btAdapterImpl;
#else
    #include "BtMgrAdapterImpl.h"
    static BtMgrAdapterImpl g_btAdapterImpl;
#endif

static IBtAdapter& getImpl() {
    if (!BtAdapter::impl) BtAdapter::impl = &g_btAdapterImpl;
    return *BtAdapter::impl;
}
```

---

## Component: BtSdkAdapterImpl (SDK path)

Owns the `bluetooth::Manager` and the default `bluetooth::Adapter`. Provides the `IBtAdapter` implementation for SDK builds. All `Bluetooth.cpp` calls route here when `BLUETOOTH_USE_SDK` is defined.

**Lifecycle:**

```
BtSdkAdapterImpl::init(service):
  1. Construct bluetooth::Manager(
       AuthorisationMode::ExternalAuthorisation,
       [this](AuthorisationType t, shared_ptr<Device> d) { return authBridge.onAuthRequest(t, d); },
       LogLocation::LogRedirect,
       logRedirectInstance
     )
  2. manager->getDefaultAdapter(adapter)   ← fatal if no adapter found
  3. adapter->registerForEvents([this](AdapterEvent e, AdapterEventData d) { eventBridge.onAdapterEvent(e, d); })
  4. Enumerate existing paired devices, register device events for each
  5. Return success

BtSdkAdapterImpl::deinit():
  1. adapter->unregisterForEvents()
  2. Unregister device events for all tracked devices
  3. Destroy Manager (destructor handles BlueZ agent unregistration)
```

**Fatal failure condition:** If `getDefaultAdapter` fails, `BtSdkAdapterImpl::init` returns an error string and `Bluetooth::Initialize` returns it as failure.

---

## Component: BtMgrAdapterImpl (BTMgr fallback path)

Provides the `IBtAdapter` implementation for BTMgr builds (when `BluetoothSDK` is not found at CMake configure time). Wraps all `BTRMGR_*` C API calls and the IARM event registration lifecycle. All logic is extracted from the old `Bluetooth.cpp` — no logic changes, only relocation into the adapter pattern.

**Lifecycle:**

```
BtMgrAdapterImpl::init(service):
  1. BTRMGR_RegisterForCallbacks(Utils::IARM::NAME)
  2. BTRMGR_RegisterEventCallback(staticEventCallback)
     where staticEventCallback maps BTRMGR_EventMessage_t to BtEventCallbacks
  3. Return success

BtMgrAdapterImpl::deinit():
  1. BTRMGR_UnRegisterFromCallbacks(Utils::IARM::NAME)
```

**Handle derivation:** `BtMgrAdapterImpl` derives handle strings inline using the same `strtoll(mac_no_colons, NULL, 16)` formula (matching BTMgr's `btrCore_GenerateUniqueDeviceID`). It does not use `DeviceRegistry` — that class is SDK-path only.

**Audio operations:** All five audio methods (`setAudioStream`, `setAudioControlCommand`, `setDeviceVolumeMute`, `getDeviceVolumeMute`, `getMediaTrackInfo`) have working implementations using BTRMGR calls. These are extracted from the legacy `Bluetooth.cpp` with minimal changes.

**Compile isolation:** `BtMgrAdapterImpl.h/.cpp` are only compiled when `BTMGR_FOUND` (BTMgr path). No BTRMGR headers enter SDK builds. No SDK headers enter BTMgr builds.

---

## Component: DeviceRegistry

Provides stable numeric device handle identity for backward compatibility with existing clients and PersistentStore data.

**Handle derivation:** ~~INV-1 resolved.~~ Handle values are derived deterministically from MAC address by stripping colons and parsing the resulting 12-hex-digit string as a base-16 integer via `strtoll`. This exactly matches BTMgr's `btrCore_GenerateUniqueDeviceID`. No migration of PersistentStore handle values is required.

```cpp
uint64_t deriveHandle(const std::string& mac) {
    // Strip colons from "AA:BB:CC:DD:EE:FF" → "AABBCCDDEEFF", parse as hex.
    char hexStr[13] = {};
    // ... copy 12 hex chars at positions 0,1,3,4,6,7,9,10,12,13,15,16
    return static_cast<uint64_t>(strtoll(hexStr, nullptr, 16));
}
```

**Identity map (runtime):**

```
DeviceRegistry:
  map<string, string>  macToHandle       // "AA:BB:CC:DD:EE:FF" → "187723572702975"
  map<string, string>  handleToTypeStr   // "187723572702975" → "HEADPHONES"
```

Note: `DeviceRegistry` is SDK-path only. `BtMgrAdapterImpl` derives handles inline without a registry object.

---

## Component: DeviceTypeClassifier

Infers the device type string from `DeviceProperties`. Called when a device is first seen and result cached in `DeviceRegistry`.

Output strings **must exactly match** BTMgr's `btrMgr_MapDeviceTypeFromCore` — PersistentStore `deviceType` values and connection dispatch logic both depend on these specific strings. BTMgr collapses several CoD classes into a single string; the classifier must replicate this.

**Classification table** (input: `DeviceProperties::classOfDevice` + `appearance` + `uuids`; evaluated in priority order):

| BTCore device class / BLE appearance | Output type string | Notes |
|---|---|---|
| DC_Tablet | `"TABLET"` | |
| DC_SmartPhone | `"SMARTPHONE"` | |
| DC_WearableHeadset | `"WEARABLE HEADSET"` | |
| DC_Handsfree | `"HANDSFREE"` | |
| DC_Loudspeaker | `"LOUDSPEAKER"` | |
| DC_Headphones | `"HEADPHONES"` | |
| DC_PortableAudio | `"LOUDSPEAKER"` | ← collapsed (not "PORTABLE AUDIO") |
| DC_CarAudio | `"LOUDSPEAKER"` | ← collapsed (not "CAR AUDIO") |
| DC_HIFIAudioDevice | `"LOUDSPEAKER"` | ← collapsed (not "HIFI AUDIO DEVICE") |
| DC_Tile / BLE Tag/Keyring appearance | `"LE TILE"` | |
| DC_XBB | `"XBB"` | |
| DC_HID_Keyboard / DC_HID_Mouse / DC_HID_MouseKeyBoard / DC_HID_AudioRemote / DC_HID_Joystick | `"HUMAN INTERFACE DEVICE"` | All HID subtypes collapse |
| DC_HID_GamePad / BLE HID gamepad appearance (0x3C4) | `"HUMAN INTERFACE DEVICE"` | Gamepad collapses into HID |
| DC_Unknown / no match | `"UNKNOWN DEVICE"` | |

**Classification priority order:**
1. BLE Appearance value (`DeviceProperties::appearance`) — use for LE devices
2. Classic BT Class of Device (`DeviceProperties::classOfDevice`) — use for classic devices
3. Service UUID fallback (`DeviceProperties::uuids`): AudioSink → `"HEADPHONES"`, AudioSource → `"SMARTPHONE"`, HumanInterfaceDevice UUID → `"HUMAN INTERFACE DEVICE"`
4. Default: `"UNKNOWN DEVICE"`

---

## Component: EventBridge

Maps `AdapterEvent` and `DeviceEvent` from the SDK into the plugin's existing JSON-RPC notification calls.

### AdapterEvent → Notification

| SDK AdapterEvent | JSON-RPC notification | Details |
|---|---|---|
| `DiscoveryStarted` | `onStatusChanged` | `newStatus: DISCOVERY_STARTED` |
| `DiscoveryStopped` | `onStatusChanged` | `newStatus: DISCOVERY_COMPLETED`. Also stops internal discovery timer (see note below). |
| `PoweredOn` | `onStatusChanged` | `newStatus: HARDWARE_AVAILABLE` or `SOFTWARE_ENABLED` (adapter power state determines which) |
| `PoweredOff` | `onStatusChanged` | `newStatus: SOFTWARE_DISABLED` |
| `DeviceDiscovered` | `onDiscoveredDevice` | Device properties populated from `getAllProperties()`; type string from DeviceRegistry |
| `DeviceDisappeared` | `onDiscoveredDevice` (discoveryType: `LOST`) | For paired disappeared device → also emit `onDeviceLost` |

**Discovery timer note:** The current plugin runs a software timer to stop discovery after a timeout. With the SDK, `DiscoveryStopped` fires when the adapter actually stops scanning (either from stopScan() or BlueZ internal). The plugin must still call `stopScan()` when the timer fires (unchanged), but the `DISCOVERY_COMPLETED` notification is now driven by the `DiscoveryStopped` event rather than by the plugin timer directly.

### DeviceEvent → Notification

| SDK DeviceEvent | JSON-RPC notification | `newStatus` | `paired` | `connected` |
|---|---|---|---|---|
| `Paired` | `onStatusChanged` | `PAIRING_CHANGE` | `true` | from properties |
| `Unpaired` | `onStatusChanged` | `PAIRING_CHANGE` | `false` | `false` |
| `Connected` | `onStatusChanged` | `CONNECTION_CHANGE` | `true` | `true` |
| `Disconnected` | `onStatusChanged` | `CONNECTION_CHANGE` | `true` | `false` |

**Failure events:** ~~INV-3 confirmed.~~ The SDK emits no failure events — `DeviceEvent` has only `Paired`, `Connected`, `Disconnected`, `ServicesResolved`, `ServicesUnresolved`, `Unpaired`. Failures surface only via the `Status` return from sync calls. The plugin MUST use `pair(sync=true)` / `connect(sync=true)` and emit `onRequestFailed` when Status is non-success. If the SDK team adds `DeviceEvent::PairFailed` / `ConnectFailed` in future, the EventBridge can be updated to handle them directly.

### Media Events (AUDIO_SUPPORT)

Media events (track started/paused/stopped/changed, playback progress, device media status) are assumed to be surfaced by the SDK's AUDIO_SUPPORT module as device events or via a separate media event callback. Exact event type mapping to `onPlaybackChange`, `onPlaybackProgress`, `onPlaybackNewTrack`, `onDeviceMediaStatus` must be confirmed when AUDIO_SUPPORT API is finalized.

---

## Component: AuthBridge

The SDK's `Agent::AuthorizeService` / `RequestConfirmation` / `RequestAuthorization` methods are confirmed to be **pure pass-through with cooldown check only** — no device-type policy, no auto-accept logic for paired audio/HID devices, no reconnection hold-off timers. The AuthBridge must replicate the full policy logic previously buried in BTMgr's `btrMgr_ConnectionInAuthenticationCb`.

### Auto-Accept Policy

BTMgr auto-accepted most device types silently. The plugin only received `BTRMGR_EVENT_RECEIVED_EXTERNAL_CONNECT_REQUEST` for smartphones, tablets, and some LE devices. The AuthBridge must implement equivalent policy:

```
SDK authManagerCallback(type, device)
             │
    ┌────────┴──────────────────────────────────────┐
    │                                               │
    ▼                                               ▼
AuthorisationType::PairingRequest          AuthorisationType::ConnectionRequest
    │                                               │
 Emit onPairingRequest                    Classify device type
 Wait for respondToEvent                            │
 Return bool                         ┌──────────────┼───────────────────┐
                                     │              │                   │
                                     ▼              ▼                   ▼
                               AUDIO DEVICE    HID DEVICE        SMARTPHONE /
                               (headphones,    (keyboard,         TABLET /
                                speakers, etc.) mouse, joystick)  LE device
                                     │              │                   │
                             if paired +       if paired +        Emit
                             not in           not recently        onConnectionRequest
                             cooldown:        disconnected:       Wait for
                             auto-accept      auto-accept         respondToEvent
                                                                  Return bool
```

**Audio device auto-accept conditions** (mirrors `btrMgr_ConnectionInAuthenticationCb`):
- Device is in Paired state
- Device is not in cooldown (SDK already checks this)
- Device was not the last disconnected device within hold-off window

**HID device auto-accept conditions**:
- Device is in Paired state
- Device is not in cooldown
- Device is not currently being paired/connected by plugin-initiated flow

**Playback request special case** (`AuthorisationType::ConnectionRequest` for audio-streaming devices): When the client accepts via `respondToEvent`, the AuthBridge must also trigger audio streaming via the AUDIO_SUPPORT API. BTMgr did this inside `BTRMGR_SetEventResponse` — the AuthBridge now owns it.

### Auth Threading

~~INV-2 resolved.~~ Thunder JSON-RPC (`respondToEvent`) is delivered over Thunder's own WebSocket/HTTP transport — independent of D-Bus. Blocking the D-Bus event loop thread during the auth wait does **not** block `respondToEvent` delivery.

For escalate-to-client cases the AuthBridge uses a simple polling/condvar wait on the D-Bus dispatch thread (same pattern as BTMgr's `btrMgr_IncomingConnectionAuthentication`, which polled every 500ms for up to 40 seconds on its auth thread). The 30-second bounded wait is acceptable; other Bluetooth D-Bus events are buffered during this window, which is the same behavior BTMgr produced.

**Implementation:** Per-device `condition_variable` + `mutex` + `bool accepted` triad, signalled by `onRespondToEvent`. No `std::async` or separate thread needed.

---

## Discovery: Profile String → SDK ScanFilter

The plugin's `startDeviceDiscovery(profile)` method uses profile strings to drive scan filtering. These map to `bluetooth::ScanFilter` as follows:

| Profile keyword(s) present | `ScanFilter::type` | `ScanFilter::uuids` |
|---|---|---|
| Audio keywords + HID keywords | `AllDevices` | AudioSink + HumanInterfaceDevice UUIDs |
| Audio keywords only (LOUDSPEAKER / HEADPHONES / WEARABLE HEADSET / HIFI AUDIO DEVICE) | `ClassicOnly` | AudioSink, AdvancedAudioDistribution |
| SMARTPHONE or TABLET | `ClassicOnly` | AudioSource |
| HID keywords only (KEYBOARD / MOUSE / JOYSTICK) | `ClassicOnly` | HumanInterfaceDevice |
| LE TILE or LE | `LeOnly` | (none) |
| DEFAULT or no match | `AllDevices` | (none) |

Default profile (no profile parameter): equivalent to Audio+HID → `AllDevices` with AudioSink + HumanInterfaceDevice UUIDs.

**Discovery stop:** `Adapter::stopScan()` takes no operation type parameter; the hardcoded `AUDIO_OUTPUT` op type issue in the current BTRMGR stop call disappears entirely.

---

## Connection Dispatch Simplification

The current dispatch table selects between `StartAudioStreamingOut`, `StartAudioStreamingIn`, and `ConnectToDevice` based on device type string. With the SDK, `Device::connect()` is used for **all** device types. BlueZ negotiates the appropriate A2DP/HFP/HID profile automatically.

Connection type dispatch table is **eliminated**. The plugin calls `device->connect()` for all device types. Disconnection similarly uses `device->disconnect()` for all device types.

The `setAudioStream(PRIMARY/AUXILIARY)` routing is delegated to the SDK's AUDIO_SUPPORT module.

---

## Audio Interface

The five audio operations are declared as pure virtual methods in `IBtAdapter`. `Bluetooth.cpp` delegates directly to `m_btAdapter` for all five — no `#ifdef` in `Bluetooth.cpp`.

### IBtAdapter audio additions

```cpp
// Plain structs — no WPEFramework types in IBtAdapter.h
struct BtMediaTrackInfo {
    std::string album, genre, title, artist;
    uint32_t    duration{0}, trackNumber{0}, numberOfTracks{0};
};
struct BtDeviceVolumeMute {
    uint8_t volume{0};
    bool    mute{false};
    bool    valid{false};   // false on BTRMGR/SDK failure
};

// Note: deviceID is currently vestigial in setAudioStream — the operation
// is adapter-level (BTRMGR_SetAudioStreamingOutType uses adapter 0, not a device handle).
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

`Bluetooth.cpp` builds `JsonObject` return values from the plain structs (same pattern as `BtDeviceProperties` → `getDeviceInfo()`).

### BtSdkAdapterImpl audio

All five methods return empty/false stubs gated by `#ifdef BLUETOOTH_AUDIO_SUPPORT` pending T-7. The `BLUETOOTH_AUDIO_SUPPORT` flag only appears in `BtSdkAdapterImpl.cpp`, not in `Bluetooth.cpp`.

| Plugin operation | Expected AUDIO_SUPPORT API (T-7, pending INV-4) |
|---|---|
| `setAudioStream` | SDK audio routing selector |
| `setAudioControlCommand` | SDK media control command |
| `getMediaTrackInfo` | SDK media track info query |
| `setDeviceVolumeMute` | SDK volume/mute set |
| `getDeviceVolumeMute` | SDK volume/mute get |

### BtMgrAdapterImpl audio

All five methods have working implementations extracted from the legacy `Bluetooth.cpp`:

| IBtAdapter method | BTRMGR call |
|---|---|
| `setAudioStream` | `BTRMGR_SetAudioStreamingOutType(0, streamType)` |
| `setAudioControlCommand` | `BTRMGR_MediaControl(0, handle, ctrl)` / `BTRMGR_StartAudioStreamingIn` |
| `setDeviceVolumeMute` | `BTRMGR_SetDeviceVolumeMute(0, handle, opType, vol, mute)` |
| `getDeviceVolumeMute` | `BTRMGR_GetDeviceVolumeMute(0, handle, opType, &vol, &mute)` |
| `getMediaTrackInfo` | `BTRMGR_GetMediaTrackInfo(0, handle, &info)` |

---

## Initialization / Deinitialization Sequence

### New Initialize() sequence

```
1. Register JSON-RPC methods (unchanged)
2. m_btAdapter.init(service)          ← dispatches to BtSdkAdapterImpl or BtMgrAdapterImpl
   SDK path:                           BTMgr path:
     a. Construct bluetooth::Manager     a. BTRMGR_RegisterForCallbacks()
     b. getDefaultAdapter(adapter)       b. BTRMGR_RegisterEventCallback()
     c. adapter->registerForEvents()
     d. Register device events
3. PowerManager init (non-fatal — unchanged)
4. BluetoothDeviceManager::init(service) ← FATAL if fails (unchanged)
5. disconnectExternallyConnectedDevices() (unchanged logic, uses m_btAdapter)
```

### New Deinitialize() sequence

```
1. BluetoothDeviceManager::deinit() (unchanged)
2. PowerManager unregister + reset (unchanged)
3. m_btAdapter.deinit()               ← dispatches to BtSdkAdapterImpl or BtMgrAdapterImpl
   SDK path:                           BTMgr path:
     a. Unregister device events         a. BTRMGR_UnRegisterFromCallbacks()
     b. adapter->unregisterForEvents()
     c. Destroy Manager
```

**Ordering note:** The singleton null-before-unregister ordering in the current code (prevents late events from crashing) is handled by the SDK's own lifetime management. When the Manager is destroyed, no further event callbacks fire.

---

## D-Bus Event Loop Threading

The bluetooth-sdk uses sdbus-c++ which requires a D-Bus event dispatch loop.

**Confirmed:** `BtSdkAdapter` must own a dedicated event loop thread (`std::thread` running `sdbus::IConnection::enterEventLoopAsync()`, joined on deinit) unless Thunder already provides one it can attach to.

**INV-2 (partial):** Whether Thunder provides a shared D-Bus event loop remains to be confirmed. The auth threading strategy (see AuthBridge section) depends on whether `respondToEvent` JSON-RPC calls arrive via D-Bus or Thunder's own IPC. If Thunder IPC is D-Bus-independent, blocking the D-Bus thread for the short auth-wait window is workable. If not, `std::async` dispatch for client-escalation cases is required.

---

## Handle Migration Concern

~~INV-1 resolved — no migration required.~~ BTCore's confirmed formula (`strtoll(mac_without_colons, NULL, 16)`) produces identical handle values to what BTMgr stored in PersistentStore. Existing `deviceID` values survive the SDK transition without any re-keying.

---

## BluetoothDeviceManager BTMgr Call Sites

`BluetoothDeviceManager` has three internal BTMgr call sites that must be updated. Its public interface is unchanged.

| Method | Current BTMgr call | SDK replacement |
|---|---|---|
| `addDevice(deviceID)` | `BTRMGR_GetDeviceProperties(0, handle, &prop)` to populate `deviceAddr`, `friendlyName`, `deviceType` | `device->getAllProperties()` via `BtSdkAdapter::getDeviceByHandle(deviceID)` |
| `updateCacheFromDevice()` | `BTRMGR_GetPairedDevices(0, &list)` to enumerate and reconcile paired device list | `adapter->getDevices(DeviceState::Paired)` + `device->getAllProperties()` per device |
| `writeCacheFromFilesystemPersistence()` (migration flag) | `BTRMGR_GetPairedDevices(0, &list)` to build MAC→handle mapping for AC1 import | `adapter->getDevices(DeviceState::Paired)` + `device->address()` per device |

`BtSdkAdapter` must expose a `getDeviceByHandle(handleStr)` lookup method (uses `DeviceRegistry`) so `BluetoothDeviceManager` can retrieve a `Device` by its numeric handle string without taking a direct SDK dependency.

---

## Impact on L1 Tests

### Mock Infrastructure Pattern

The testframework uses a **static-impl dispatch pattern** for all external library mocks. The BTMgr mock demonstrates the pattern:

```
btmgr.h (testframework)   BtmgrImpl (pure virtual) + Btmgr (dispatch wrapper with setImpl())
btmgr.cpp (testframework)  dispatch implementations; setImpl() validates single-assignment
btmgrMock.h               BtmgrImplMock : BtmgrImpl with MOCK_METHOD for each function
```

In test builds, the include path ensures testframework's `btmgr.h` is resolved before the real BTMgr header. Tests call `Btmgr::setImpl(p_btmgrMock)` in fixture setup and `Btmgr::setImpl(nullptr)` in teardown. `BTRMGR_RegisterEventCallback` is also a `MOCK_METHOD` — tests can capture the registered callback and invoke it directly to inject events.

**The equivalent for `BtAdapter` uses the same pattern.** Three testframework files are needed:

```
BtAdapter.h (testframework)   IBtAdapter (pure virtual) + BtAdapter (dispatch wrapper)
BtAdapter.cpp (testframework)  dispatch implementations; no bluetooth-sdk dependency
BtAdapterMock.h               BtAdapterImplMock : IBtAdapter with MOCK_METHODs
                               + event injection helpers (fireAdapterEvent, fireDeviceEvent)
```

In test builds, testframework's `BtAdapter.h` shadows the plugin's `BtAdapter.h` via include path ordering. `Bluetooth.h` still holds `BtAdapter m_btAdapter` as a value member — no change.

**Production build:** `BtAdapter.h/.cpp` in plugin provide the thin dispatch wrapper; `BtSdkAdapterImpl.cpp` (SDK path) or `BtMgrAdapterImpl.cpp` (BTMgr path) provides the backend implementation.

**Test build:** `BtAdapter.h/.cpp` from testframework replace the plugin's header/dispatcher; neither backend impl is compiled; `TestMocklib` provides the dispatch stub.

### Event Injection (New Capability vs BTMgr)

With BTMgr, tests injected events by capturing `BTRMGR_RegisterEventCallback` and invoking the callback directly with a crafted `BTRMGR_EventMessage_t`. The SDK refactor improves on this:

`BtSdkAdapterImplMock` captures the EventBridge callbacks during its `init()` implementation and exposes `fireAdapterEvent()`/`fireDeviceEvent()` helpers. Tests call these to simulate SDK events flowing into the plugin:

```cpp
// Test fires a Connected event for a paired device
AdapterEventData data;
data.device = nullptr;  // adapter-level event
p_btSdkMock->fireAdapterEvent(AdapterEvent::DiscoveryStarted, data);
// → verify onStatusChanged(DISCOVERY_STARTED) notification was emitted
```

### File Locations

| File | Repo | Purpose |
|---|---|---|
| `IBtAdapter` + `BtAdapter` dispatch (test stub) | `entservices-testframework/Tests/mocks/BtAdapter.h/.cpp` | Shadows plugin `BtAdapter.h` in test builds; added to `TestMocklib` sources |
| `BtAdapterImplMock` | `entservices-testframework/Tests/mocks/BtAdapterMock.h` | GoogleMock with event injection; +audio MOCK_METHODs |
| `BtAdapter` production dispatch | `entservices-connectivity/Bluetooth/BtAdapter.h/.cpp` | Constructs `g_btAdapterImpl` (SDK or BTMgr) on first `init()` |
| `BtSdkAdapterImpl` | `entservices-connectivity/Bluetooth/BtSdkAdapterImpl.h/.cpp` | Wraps `bluetooth::Manager/Adapter/Device`; SDK builds only |
| `BtMgrAdapterImpl` | `entservices-connectivity/Bluetooth/BtMgrAdapterImpl.h/.cpp` | Wraps BTRMGR C API; BTMgr builds only |

- `DeviceRegistry`, `DeviceTypeClassifier` unit tests: standalone, no mock needed
- `EventBridge`, `AuthBridge` tests: use `BtSdkAdapterImplMock` event injection
- `BluetoothDeviceManager` tests: use mock adapter via `getDeviceByHandle()` mock

---

## Open Questions Summary

| # | Question | Status | Needed for |
|---|---|---|---|
| 1 | BTRMgrDeviceHandle derivation formula | **CLOSED** — strip colons, `strtoll` base-16. PS data portable, no migration. | — |
| 2 | Does Thunder JSON-RPC use D-Bus or independent IPC? | **CLOSED** — Thunder uses WebSocket/HTTP, D-Bus-independent. Polling on D-Bus thread during auth wait is safe. | — |
| 3 | Will SDK add `DeviceEvent::PairFailed` / `ConnectFailed`? | **OPEN** — confirmed absent today. Sync call + emit pattern is the fallback regardless. | EventBridge (nice-to-have improvement) |
| 4 | AUDIO_SUPPORT API surface and timeline | **OPEN** | Audio method implementation |
| 5 | SDK authManagerCallback async/deferred reply support | **CLOSED** — confirmed synchronous only. Auth thread must be separate or D-Bus block accepted. | — |
