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
│                                 │ calls / notifications               │
│  ┌──────────────────────────────▼─────────────────────────────┐      │
│  │  BtSdkAdapter  (new — replaces all BTRMGR_ call sites)     │      │
│  │                                                            │      │
│  │  ┌──────────────┐  ┌──────────────┐  ┌───────────────┐    │      │
│  │  │DeviceRegistry│  │ EventBridge  │  │  AuthBridge   │    │      │
│  │  │ MAC↔handle   │  │ SDK events → │  │ async client  │    │      │
│  │  │ type cache   │  │ notifications│  │ respondToEvent│    │      │
│  │  └──────────────┘  └──────────────┘  └───────────────┘    │      │
│  │                                                            │      │
│  │  ┌──────────────────────────────────────────────────────┐  │      │
│  │  │  DeviceTypeClassifier                                │  │      │
│  │  │  classOfDevice + appearance + UUIDs → type string    │  │      │
│  │  └──────────────────────────────────────────────────────┘  │      │
│  └──────────────────────────────┬─────────────────────────────┘      │
│                                 │ bluetooth-sdk API                   │
│  ┌──────────────────────────────▼─────────────────────────────┐      │
│  │  bluetooth-sdk                                             │      │
│  │  bluetooth::Manager → bluetooth::Adapter → bluetooth::Device│     │
│  │  (AUDIO_SUPPORT module assumed complete)                   │      │
│  └──────────────────────────────┬─────────────────────────────┘      │
│                                 │ sdbus-c++ / D-Bus                   │
└─────────────────────────────────┼────────────────────────────────────┘
                                  │
                               BlueZ
```

`BluetoothDeviceManager` owns PersistentStore access for paired device metadata. Its public interface is unchanged, but **three internal BTMgr call sites require SDK replacements** — see the BluetoothDeviceManager section below.

---

## Component: BtSdkAdapter

Owns the `bluetooth::Manager` and the default `bluetooth::Adapter`. Provides the same operational interface that the `Bluetooth` plugin previously obtained from `BTRMGR_*` calls. All plugin methods that previously called BTRMGR functions call `BtSdkAdapter` methods instead.

**Lifecycle:**

```
BtSdkAdapter::init(service):
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

BtSdkAdapter::deinit():
  1. adapter->unregisterForEvents()
  2. Unregister device events for all tracked devices
  3. Destroy Manager (destructor handles BlueZ agent unregistration)
```

The IARM `init()`/`BTRMGR_RegisterForCallbacks()`/`BTRMGR_UnRegisterFromCallbacks()` lifecycle is fully replaced by steps 1–3 above.

**Fatal failure condition:** If `getDefaultAdapter` fails (no Bluetooth hardware), `BtSdkAdapter::init` returns error and plugin Initialize returns a non-empty error string. Equivalent to the current BTRMGR_RegisterForCallbacks failure path.

---

## Component: DeviceRegistry

Provides stable numeric device handle identity for backward compatibility with existing clients and PersistentStore data.

**Handle derivation:** Handle values are derived deterministically from MAC address using FNV-1a 64-bit hash. This ensures the same device always gets the same handle across restarts without requiring stored state. The collision probability over the small device population encountered in practice is negligible.

```
uint64_t deriveHandle(const std::string& mac) {
    // FNV-1a 64-bit over the 6 bytes of the MAC address
    // (strips colons, hashes raw bytes)
}
```

**Identity map (runtime):**

```
DeviceRegistry:
  map<string, shared_ptr<Device>> handleToDevice   // "12345678" → Device ptr
  map<string, string>             macToHandle       // "AA:BB:CC:DD:EE:FF" → "12345678"
  map<string, string>             handleToTypeStr   // "12345678" → "HEADPHONES"
```

**Existing PersistentStore data:** The `deviceID` values already stored in `Bluetooth/deviceInfo` in PersistentStore were generated by BTMgr from the same underlying MAC addresses. FNV-1a must produce the same values as BTMgr's handle derivation, **or** a one-time migration of stored handles must be performed at plugin init.

> **Open question**: Confirm whether BTMgr derives `BTRMgrDeviceHandle` via FNV-1a over MAC bytes, or uses a different scheme. If the scheme differs, a handle re-derivation migration is required at first boot after the SDK transition.

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

## Audio (AUDIO_SUPPORT assumed complete)

Audio operations that currently go to `BTRMGR_StartAudioStreamingOut/In`, `BTRMGR_MediaControl`, `BTRMGR_GetMediaTrackInfo`, and `BTRMGR_GetDeviceVolumeMute/SetDeviceVolumeMute` are replaced by calls into the SDK's AUDIO_SUPPORT module. The exact API surface of AUDIO_SUPPORT is to be confirmed when that module is available. The following is the expected mapping:

| Plugin operation | Expected AUDIO_SUPPORT API |
|---|---|
| `setAudioStream(PRIMARY/AUXILIARY)` | SDK audio routing selector |
| `sendAudioPlaybackCommand(PLAY/PAUSE/RESUME/STOP/SKIP_NEXT/SKIP_PREV/MUTE/etc.)` | SDK media control command |
| `getAudioInfo` (track info) | SDK media track info query |
| `setDeviceVolumeMuteInfo` | SDK volume/mute set |
| `getDeviceVolumeMuteInfo` | SDK volume/mute get |

Until AUDIO_SUPPORT is available, audio methods continue to fail gracefully (same as current `RESTART` behavior). A `BLUETOOTH_AUDIO_SUPPORT` compile-time flag mirrors the SDK's own `AUDIO_SUPPORT` flag and gates these method implementations.

---

## Initialization / Deinitialization Sequence

### New Initialize() sequence

```
1. Register JSON-RPC methods (unchanged)
2. BtSdkAdapter::init(service)
   a. Construct bluetooth::Manager(ExternalAuthorisation, authCallback, ...)
   b. getDefaultAdapter(adapter)  ← FATAL if fails (no adapter)
   c. adapter->registerForEvents(adapterEventCallback)
   d. Register DeviceEvent callbacks for existing paired devices
3. PowerManager init (non-fatal — unchanged)
4. BluetoothDeviceManager::init(service) ← FATAL if fails (unchanged)
5. disconnectExternallyConnectedDevices() (unchanged logic, uses BtSdkAdapter)
```

Replaces: `Utils::IARM::init()`, `BTRMGR_RegisterForCallbacks()`, `BTRMGR_RegisterEventCallback()`.

### New Deinitialize() sequence

```
1. BluetoothDeviceManager::deinit() (unchanged)
2. PowerManager unregister + reset (unchanged)
3. BtSdkAdapter::deinit()
   a. Unregister device event callbacks
   b. adapter->unregisterForEvents()
   c. Destroy Manager (BlueZ agent unregistered automatically)
```

Replaces: `BTRMGR_UnRegisterFromCallbacks()`.

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

- Remove dependency on `entservices-testframework/Tests/mocks/btmgr.h`.
- Add a `BtSdkMock` that implements the bluetooth-sdk public interfaces using GoogleMock.
- All existing L1 test scenarios remain; event injection changes from `BTRMGR_EventMessage_t` structs to SDK event emission via mock.
- AuthBridge scenarios: auto-accept for paired audio/HID devices; escalate-to-client for smartphones/LE; timeout auto-reject; playback-accept triggers audio start.
- AuthBridge threading: verify D-Bus event loop is not blocked during client-escalation wait.
- DeviceTypeClassifier: unit tests covering all CoD/Appearance/UUID paths.
- DeviceRegistry: unit tests for handle derivation stability and bidirectional lookup.

---

## Open Questions Summary

| # | Question | Status | Needed for |
|---|---|---|---|
| 1 | BTRMgrDeviceHandle derivation formula | **CLOSED** — strip colons, `strtoll` base-16. PS data portable, no migration. | — |
| 2 | Does Thunder JSON-RPC use D-Bus or independent IPC? | **CLOSED** — Thunder uses WebSocket/HTTP, D-Bus-independent. Polling on D-Bus thread during auth wait is safe. | — |
| 3 | Will SDK add `DeviceEvent::PairFailed` / `ConnectFailed`? | **OPEN** — confirmed absent today. Sync call + emit pattern is the fallback regardless. | EventBridge (nice-to-have improvement) |
| 4 | AUDIO_SUPPORT API surface and timeline | **OPEN** | Audio method implementation |
| 5 | SDK authManagerCallback async/deferred reply support | **CLOSED** — confirmed synchronous only. Auth thread must be separate or D-Bus block accepted. | — |
