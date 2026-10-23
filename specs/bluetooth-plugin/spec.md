# Bluetooth Plugin Specification

## Scope
This specification defines the expected behavior of the Bluetooth Thunder plugin exposed as callsign org.rdk.Bluetooth.

It covers:
- JSON-RPC method contract and behavior
- Event/notification contract
- Lifecycle behavior at plugin initialization and shutdown
- Persistent metadata behavior for paired devices
- Power mode transition behavior

It does not redefine BTRMGR internals.

## ADDED Requirements

### Requirement: Plugin Identity And Surface
The plugin MUST expose service callsign org.rdk.Bluetooth and support API version major 1.

The plugin MUST provide JSON-RPC methods for:
- Adapter control and status: getApiVersionNumber, enable, disable, getName, setName, isDiscoverable, setDiscoverable
- Discovery and pairing lifecycle: startScan, stopScan, getDiscoveredDevices, getPairedDevices, getConnectedDevices, pair, unpair, connect, disconnect
- Media and interaction: setAudioStream, getDeviceInfo, getAudioInfo, sendAudioPlaybackCommand, respondToEvent
- Audio endpoint controls: setDeviceVolumeMuteInfo, getDeviceVolumeMuteInfo
- Device metadata controls: setAutoConnect, getAutoConnect

#### Scenario: API version query
- WHEN a client invokes getApiVersionNumber
- THEN the plugin returns success=true and version=1

#### Scenario: Method availability
- WHEN clients invoke the methods listed above
- THEN each method is registered and routable via org.rdk.Bluetooth.1.

### Requirement: Initialization And Deinitialization
On Initialize(service), the plugin MUST:
- Register all JSON-RPC methods listed in this spec
- Initialize IARM
- Register BTRMGR event callback handling
- Acquire org.rdk.PowerManager interface and register power mode notifications (if available)
- Initialize BluetoothDeviceManager using the provided service shell

On Deinitialize(service), the plugin MUST:
- Deinitialize BluetoothDeviceManager
- Unregister PowerManager notifications and release/reset PowerManager interface
- Unregister BTRMGR callback handling

#### Scenario: Power manager available
- GIVEN org.rdk.PowerManager can be acquired
- WHEN Initialize executes
- THEN the plugin registers for power notifications and handles current power state

#### Scenario: Power manager unavailable
- GIVEN org.rdk.PowerManager cannot be acquired
- WHEN Initialize executes
- THEN initialization continues and plugin logs the inability to register power notifications

### Requirement: Discovery, Pairing, And Connection Behavior
The plugin MUST bridge JSON-RPC operations to BTRMGR operations.

The plugin MUST include deviceID for operations that target a specific device.
The plugin SHOULD return failure when required parameters are missing or the underlying operation fails.

Error response handling is method-specific. The plugin is NOT required to provide a strict, global Thunder error-code mapping table per method.
The plugin is NOT required to normalize all failures into a single common response schema beyond existing success semantics.

#### Scenario: Set auto-connect for known device
- GIVEN a paired device exists in metadata cache
- WHEN setAutoConnect(deviceID, enable) is called
- THEN autoconnect status is updated in cache
- AND persistent storage is updated
- AND an auto-connect status notification is emitted

#### Scenario: Set auto-connect for unknown device
- GIVEN deviceID does not exist in metadata cache
- WHEN setAutoConnect(deviceID, enable) is called
- THEN operation returns failure with not-exist semantics

### Requirement: Event Translation Contract
The plugin MUST convert BTRMGR events into JSON-RPC notifications.

Supported notification names include:
- onStatusChanged
- onPairingRequest
- onRequestFailed
- onConnectionRequest
- onPlaybackRequest
- onPlaybackChange
- onPlaybackProgress
- onPlaybackNewTrack
- onDeviceFound
- onDeviceLost
- onDiscoveredDevice
- onDeviceMediaStatus
- onAutoConnectChanged

Event payload contract is STRICT. The plugin MUST preserve existing field names and semantics for published notifications.
Any payload changes MUST be handled via API versioning or additive optional fields that do not break existing clients.

For onStatusChanged, payload MUST include:
- newStatus
- deviceID when event is device-scoped
- paired when pairing semantics apply
- connected when connection semantics apply

For device-scoped status and discovery notifications, payload SHOULD include when available:
- name
- deviceType
- rawDeviceType
- rawBleDeviceType
- lastConnectedState
- autoconnect (when metadata exists)

#### Scenario: Discovery completion
- WHEN BTRMGR emits device discovery complete
- THEN plugin emits onStatusChanged with newStatus indicating discovery completion

#### Scenario: Connection state changed
- WHEN BTRMGR emits connection or disconnection complete for a device
- THEN plugin emits onStatusChanged with updated paired and connected fields
- AND includes autoconnect state when available in metadata

### Requirement: Paired Device Metadata Persistence
Plugin metadata persistence MUST use PersistentStore callsign org.rdk.PersistentStore with:
- namespace: Bluetooth
- key: deviceInfo

The stored value MUST be a bare JSON array (no top-level wrapper object). Each element in the array represents one cached paired device and MUST include, at minimum:

| Field | Type | Description |
| --- | --- | --- |
| `deviceID` | string | BTRMGR device handle (numeric handle serialized as string), used as the cache key |
| `deviceType` | string | BTRMGR device type string (e.g. "HEADPHONES", "HUMAN INTERFACE DEVICE") |
| `autoconnect` | integer | Auto-connect status enum: 0 = disabled, 1 = enabled, 2 = unset |
| `lastConnectTimeUtc` | string | UTC timestamp of last connection as a decimal string (empty string when unavailable) |
| `lastVolumeSetting` | integer | Last known volume level |

Note: `docs/paired_bluetooth_devices.schema.json` defines the format used by the filesystem persistence layer (`BluetoothPersistenceAdapter`) that is active only under the `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` build flag. That schema does NOT describe the PersistentStore format above.

Additional fields MAY be introduced as additive optional fields while preserving read compatibility.

On manager initialization, implementation MUST:
- Load cache from storage when present
- Reconcile cache with currently paired platform devices
- Persist reconciled cache

#### Scenario: Store unavailable
- GIVEN PersistentStore interface cannot be acquired
- WHEN metadata manager reads or writes data
- THEN operation returns error and no crash occurs

#### Scenario: AC1 migration from filesystem persistence source
- GIVEN PersistentStore key Bluetooth/deviceInfo does not exist
- WHEN migration logic reads local AS payload
- THEN invalid source values preserve cache values when available

#### Scenario: AC2 rollback synchronization
- GIVEN plugin cache persistence succeeds
- WHEN rollback path writes AS payload
- THEN identity and timestamp representation remain consistent across stores

### Requirement: Power Mode Policy
The plugin MUST react to power mode transitions to enforce Bluetooth connectivity policy.

Required transition matrix:

| Transition | Required behavior |
| --- | --- |
| ON -> STANDBY | Plugin MUST apply managed disconnect policy to non-exempt connected devices. |
| STANDBY -> ON | Plugin MUST resume managed Bluetooth operation and allow normal connection behavior. |
| ON -> DEEP_SLEEP | Plugin MUST apply managed disconnect policy to non-exempt connected devices before deep sleep settles. |
| DEEP_SLEEP -> ON | Plugin MUST resume managed Bluetooth operation and allow normal connection behavior. |

HID-class devices MAY be exempt from selected disconnect behavior depending on implementation policy.

#### Scenario: Non-HID managed disconnect path
- GIVEN power transition requires disconnect handling
- WHEN connected device is not HID-exempt
- THEN plugin issues disconnect behavior for that device class

### Requirement: Adapter Indexing
Adapter index 0 is a normative platform requirement for this plugin profile.

#### Scenario: Single adapter target
- WHEN adapter operations are issued
- THEN they target adapter index 0

### Requirement: API Surface Governance
Spec artifacts are the source of truth for method and event contracts.
Documentation MUST be kept aligned with the spec and SHOULD be generated or updated from spec content.

### Requirement: BTMgr Binding Lifecycle

The plugin MUST bind to BTMgr via IARM using `BTRMGR_RegisterForCallbacks` with the plugin's IARM process name during Initialize. Registration failure is fatal: Initialize MUST return a non-empty error string and MUST NOT proceed further.

The plugin MUST register its event callback with `BTRMGR_RegisterEventCallback` only after `BTRMGR_RegisterForCallbacks` succeeds.

The event callback MUST verify the plugin singleton is non-null before dispatching. If the singleton is null the callback MUST return `BTRMGR_RESULT_INIT_FAILED` without dispatching.

On Deinitialize, the plugin MUST set the singleton instance to null before calling `BTRMGR_UnRegisterFromCallbacks`. Failure of `BTRMGR_UnRegisterFromCallbacks` during deinitialization is non-fatal.

#### Scenario: IARM registration fails at initialization
- WHEN `BTRMGR_RegisterForCallbacks` returns a non-success result
- THEN Initialize returns a non-empty error string
- AND event callback registration is not attempted

#### Scenario: Late event during shutdown
- GIVEN Deinitialize has set the singleton to null
- WHEN BTMgr fires an event before `BTRMGR_UnRegisterFromCallbacks` returns
- THEN the event callback returns `BTRMGR_RESULT_INIT_FAILED`
- AND no notification is dispatched

### Requirement: Discovery Operation Type Selection

When starting discovery, the plugin MUST select the `BTRMGR_DeviceOperationType_t` based on the profile string parameter according to the following rules, evaluated in priority order:

| Profile string contains | Selected operation type |
| --- | --- |
| Any audio keyword AND any HID keyword | `AUDIO_AND_HID` |
| LOUDSPEAKER, HEADPHONES, WEARABLE HEADSET, or HIFI AUDIO DEVICE (without HID keywords) | `AUDIO_OUTPUT` |
| SMARTPHONE or TABLET | `AUDIO_INPUT` |
| KEYBOARD, MOUSE, or JOYSTICK (without audio keywords) | `HID` |
| LE TILE or LE | `LE` |
| DEFAULT | `UNKNOWN` |
| No match | `UNKNOWN` |

Audio keywords: LOUDSPEAKER, HEADPHONES, WEARABLE HEADSET, HIFI AUDIO DEVICE.
HID keywords: KEYBOARD, MOUSE, JOYSTICK.

When `startScan` is called without a profile parameter, the plugin MUST default to the profile `"LOUDSPEAKER, HEADPHONES, WEARABLE HEADSET, HIFI AUDIO DEVICE, KEYBOARD, MOUSE, JOYSTICK"`, which selects `AUDIO_AND_HID`.

Discovery stop MUST call `BTRMGR_StopDeviceDiscovery` with `AUDIO_OUTPUT` as the operation type regardless of the operation type used to start discovery.

Note: Whether the stop operation type should track the start operation type is an open question. See open-questions.md.

#### Scenario: Audio-only scan
- WHEN `startScan` is called with profile `"HEADPHONES"`
- THEN `BTRMGR_StartDeviceDiscovery` is called with `AUDIO_OUTPUT`

#### Scenario: Combined audio and HID scan
- WHEN `startScan` is called with a profile containing both `"LOUDSPEAKER"` and `"KEYBOARD"`
- THEN `BTRMGR_StartDeviceDiscovery` is called with `AUDIO_AND_HID`

#### Scenario: Default scan when no profile given
- WHEN `startScan` is called with only a timeout parameter
- THEN `BTRMGR_StartDeviceDiscovery` is called with `AUDIO_AND_HID`

### Requirement: Connection Dispatch By Device Class

The plugin MUST select the BTMgr connection API based on the `deviceType` string provided by the client according to the following dispatch table:

| `deviceType` value | Connect call | Disconnect call |
| --- | --- | --- |
| `"LE TILE"` (exact) | `BTRMGR_ConnectToDevice` with `LE` op type | `BTRMGR_DisconnectFromDevice` |
| `"HUMAN INTERFACE DEVICE"` (exact), or contains `"KEYBOARD"`, `"MOUSE"`, or `"JOYSTICK"` | `BTRMGR_ConnectToDevice` with `HID` op type | `BTRMGR_DisconnectFromDevice` |
| `"SMARTPHONE"` or `"TABLET"` (exact) | `BTRMGR_StartAudioStreamingIn` with `AUDIO_INPUT` op type | `BTRMGR_StopAudioStreamingIn` |
| All other values (default, including all audio output device types) | `BTRMGR_StartAudioStreamingOut` with `AUDIO_OUTPUT` op type | `BTRMGR_StopAudioStreamingOut` |

The plugin uses the `deviceType` string supplied by the client at call time and does not re-query BTMgr for the device type before dispatch.

#### Scenario: LE device connect
- WHEN `connect` is called with `deviceType` `"LE TILE"`
- THEN `BTRMGR_ConnectToDevice` is called with `LE` operation type

#### Scenario: Audio output device connect
- WHEN `connect` is called with a `deviceType` that does not match LE, HID, or audio-input classes
- THEN `BTRMGR_StartAudioStreamingOut` is called with `AUDIO_OUTPUT` operation type

### Requirement: Audio Playback Command Mapping

When `sendAudioPlaybackCommand` is called, the plugin MUST translate the command string to a BTMgr call according to the following table:

| Command string | BTMgr call |
| --- | --- |
| `PLAY` | `BTRMGR_StartAudioStreamingIn` with `AUDIO_INPUT` op type |
| `PAUSE` | `BTRMGR_MediaControl` with `CTRL_PAUSE` |
| `RESUME` | `BTRMGR_MediaControl` with `CTRL_PLAY` |
| `STOP` | `BTRMGR_MediaControl` with `CTRL_STOP` |
| `SKIP_NEXT` | `BTRMGR_MediaControl` with `CTRL_NEXT` |
| `SKIP_PREV` | `BTRMGR_MediaControl` with `CTRL_PREVIOUS` |
| `RESTART` | Returns failure (not implemented) |
| `AUDIO_MUTE` | `BTRMGR_MediaControl` with `CTRL_MUTE` |
| `AUDIO_UNMUTE` | `BTRMGR_MediaControl` with `CTRL_UNMUTE` |
| `VOLUME_UP` | `BTRMGR_MediaControl` with `CTRL_VOLUMEUP` |
| `VOLUME_DOWN` | `BTRMGR_MediaControl` with `CTRL_VOLUMEDOWN` |

`PLAY` opens an audio streaming session via `StartAudioStreamingIn` rather than issuing a media control command.
`RESUME` maps to `CTRL_PLAY` because no `CTRL_RESUME` exists in the BTMgr media control API.
`RESTART` is not implemented and MUST return failure to the caller.

Note: Whether `RESTART` should be formally deprecated or implemented is an open question. See open-questions.md.

#### Scenario: Resume command mapping
- WHEN `sendAudioPlaybackCommand` is called with `RESUME`
- THEN `BTRMGR_MediaControl` is called with `CTRL_PLAY`

#### Scenario: Restart command
- WHEN `sendAudioPlaybackCommand` is called with `RESTART`
- THEN the method returns failure

### Requirement: BTMgr Event Coverage

The plugin handles a defined subset of BTMgr events and translates them to JSON-RPC notifications. Events outside this set MUST be silently dropped without emitting a notification.

Handled events and their corresponding notifications:

| BTMgr event | JSON-RPC notification | `newStatus` value (where applicable) |
| --- | --- | --- |
| `DEVICE_DISCOVERY_STARTED` | `onStatusChanged` | `DISCOVERY_STARTED` |
| `DEVICE_DISCOVERY_COMPLETE` | `onStatusChanged` | `DISCOVERY_COMPLETED` |
| `DEVICE_DISCOVERY_UPDATE` | `onDiscoveredDevice` | — |
| `DEVICE_FOUND` | `onDeviceFound` | — |
| `DEVICE_OUT_OF_RANGE` | `onDeviceLost` | — |
| `DEVICE_PAIRING_COMPLETE` | `onStatusChanged` | `PAIRING_CHANGE` |
| `DEVICE_UNPAIRING_COMPLETE` | `onStatusChanged` | `PAIRING_CHANGE` |
| `DEVICE_CONNECTION_COMPLETE` | `onStatusChanged` | `CONNECTION_CHANGE` |
| `DEVICE_DISCONNECT_COMPLETE` | `onStatusChanged` | `CONNECTION_CHANGE` |
| `DEVICE_PAIRING_FAILED` | `onRequestFailed` | `PAIRING_FAILED` |
| `DEVICE_UNPAIRING_FAILED` | `onRequestFailed` | `PAIRING_FAILED` |
| `DEVICE_CONNECTION_FAILED` | `onRequestFailed` | `CONNECTION_FAILED` |
| `RECEIVED_EXTERNAL_PAIR_REQUEST` | `onPairingRequest` | — |
| `RECEIVED_EXTERNAL_CONNECT_REQUEST` | `onConnectionRequest` | — |
| `RECEIVED_EXTERNAL_PLAYBACK_REQUEST` | `onPlaybackRequest` | — |
| `MEDIA_TRACK_STARTED` | `onPlaybackChange` (action: `started`) | — |
| `MEDIA_TRACK_PAUSED` | `onPlaybackChange` (action: `paused`) | — |
| `MEDIA_TRACK_STOPPED` | `onPlaybackChange` (action: `stopped`) | — |
| `MEDIA_PLAYBACK_ENDED` | `onPlaybackChange` (action: `ended`) | — |
| `MEDIA_TRACK_PLAYING` | `onPlaybackProgress` | — |
| `MEDIA_TRACK_POSITION` | `onPlaybackProgress` | — |
| `MEDIA_TRACK_CHANGED` | `onPlaybackNewTrack` | — |
| `DEVICE_MEDIA_STATUS` | `onDeviceMediaStatus` | — |

Events silently dropped (no notification emitted): `DEVICE_DISCONNECT_FAILED`, `DEVICE_OP_READY`, `DEVICE_OP_INFORMATION`, `MEDIA_PLAYER_NAME`, `MEDIA_PLAYER_VOLUME`, `MEDIA_PLAYER_DELAY`, `MEDIA_PLAYER_EQUALIZER_OFF/ON`, `MEDIA_PLAYER_SHUFFLE_*`, `MEDIA_PLAYER_REPEAT_*`, `MEDIA_ALBUM_INFO`, `MEDIA_ARTIST_INFO`, `MEDIA_GENRE_INFO`, `MEDIA_COMPILATION_INFO`, `MEDIA_PLAYLIST_INFO`, `MEDIA_TRACKLIST_INFO`, `BATTERY_INFO`.

Note: Whether `DEVICE_DISCONNECT_FAILED` should emit a notification is an open question. See open-questions.md.

#### Scenario: Disconnect completion
- WHEN BTMgr emits `DEVICE_DISCONNECT_COMPLETE` for a device
- THEN plugin emits `onStatusChanged` with `newStatus` `CONNECTION_CHANGE` and `connected` `false`

#### Scenario: Disconnect failure
- WHEN BTMgr emits `DEVICE_DISCONNECT_FAILED`
- THEN no notification is emitted to clients
- AND the event is silently discarded

## Constraints And Assumptions
- BTRMGR library availability is required for runtime Bluetooth operation.
- Adapter index 0 is required for this plugin profile.
- Multi-adapter support is out of scope for this specification version.

## Non-Goals
- Defining BTRMGR wire contract or internals
- Designing new API methods beyond currently implemented surface
- Defining UI-level behavior for Bluetooth clients

## Traceability (Implementation Sources)
- Bluetooth/Bluetooth.h
- Bluetooth/Bluetooth.cpp
- Bluetooth/BluetoothDeviceManager.h
- Bluetooth/BluetoothDeviceManager.cpp
- Bluetooth/Bluetooth.config
- Bluetooth/README.md
- docs/bluetooth-plugin.md
