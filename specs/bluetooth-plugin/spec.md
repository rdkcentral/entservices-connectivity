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
