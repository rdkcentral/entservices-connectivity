## ADDED Requirements

### Requirement: Legacy implementation is the contract source of truth
For every field covered by this capability, the plugin SHALL produce the same JSON key, JSON type and value as the `rdk_develop_branch` implementation, on both the BTMgr and SDK backends. Where documentation and the legacy code disagree, the legacy code SHALL prevail.

#### Scenario: Documentation disagrees with legacy code
- **WHEN** a documented behavior differs from what `rdk_develop_branch` actually emits
- **THEN** the plugin reproduces the `rdk_develop_branch` behavior

### Requirement: New-track payload field types
The `onPlaybackNewTrack` notification SHALL emit `ui32Duration`, `ui32TrackNumber` and `ui32NumberOfTracks` as JSON strings containing the decimal value, matching `getAudioInfo`.

#### Scenario: New track notification
- **WHEN** a new-track event with duration 215000, track number 3 and 12 tracks is delivered by either backend
- **THEN** the notification contains `"ui32Duration":"215000"`, `"ui32TrackNumber":"3"` and `"ui32NumberOfTracks":"12"`

### Requirement: Active state for connected devices
`getConnectedDevices` SHALL report `activeState` as the string `"1"` for every connected device on both backends.

#### Scenario: Connected device on the SDK backend
- **WHEN** a device is connected and `getConnectedDevices` is called on the SDK backend
- **THEN** that device's entry contains `"activeState":"1"`

### Requirement: Signal strength and RSSI for paired devices
For a device that is paired, `getDeviceInfo` SHALL report `rssi` as `"0"` and `signalStrength` as `"0"` on both backends, regardless of any RSSI value the stack holds.

#### Scenario: Paired device with a known RSSI
- **WHEN** `getDeviceInfo` is called for a paired device and the stack has an RSSI of -50
- **THEN** the response contains `"rssi":"0"` and `"signalStrength":"0"`

#### Scenario: Paired decision uses the pairing flag
- **WHEN** a device is connected but not paired
- **THEN** it is handled as an unpaired device for `rssi` and `signalStrength`

### Requirement: Signal strength and RSSI for unpaired devices
For a device that is not paired, `getDeviceInfo` SHALL report `rssi` as the decimal string of the raw RSSI and `signalStrength` as the decimal string of the signal level, where the level is 4 for RSSI >= -60, 3 for RSSI >= -70, 2 for RSSI >= -90, and 1 otherwise. When no RSSI is available, `rssi` SHALL be `"-2147483648"` and `signalStrength` SHALL be `"1"`. A level of 0 SHALL NOT be reported for an unpaired device.

#### Scenario: Unpaired device with strong signal
- **WHEN** `getDeviceInfo` is called for an unpaired device with RSSI -55
- **THEN** the response contains `"rssi":"-55"` and `"signalStrength":"4"`

#### Scenario: Level thresholds
- **WHEN** an unpaired device has RSSI -60, -61, -70, -71, -90 or -91
- **THEN** `signalStrength` is `"4"`, `"3"`, `"3"`, `"2"`, `"2"` and `"1"` respectively

#### Scenario: Unknown RSSI
- **WHEN** `getDeviceInfo` is called for an unpaired device and no RSSI is available
- **THEN** the response contains `"rssi":"-2147483648"` and `"signalStrength":"1"` on both backends

### Requirement: Adapter name set
`setBluetoothProperties` with a `name` SHALL set the adapter's BlueZ `Alias` on the SDK backend, truncating the name to 63 bytes as BTMgr does, and SHALL report success only when the SDK call succeeds.

#### Scenario: Name set succeeds
- **WHEN** a client sets the adapter name to a valid name on the SDK backend
- **THEN** the SDK adapter name is set and the call reports success

#### Scenario: Overlong name
- **WHEN** a client sets a name longer than 63 bytes
- **THEN** the name passed to the SDK is the first 63 bytes

#### Scenario: SDK failure
- **WHEN** the SDK name call fails
- **THEN** the plugin reports failure

### Requirement: Request-failed payload
`onRequestFailed` SHALL include `lastConnectedState` as a JSON bool, together with `paired` and `connected`, for pairing, unpairing and connection failures.

#### Scenario: Pairing failure for the last connected device
- **WHEN** the BTMgr backend reports a pairing failure for a device that was the last connected device
- **THEN** the notification contains `"lastConnectedState":true`

### Requirement: Autoconnect in status notifications
`onStatusChanged` SHALL include `autoconnect` only when `newStatus` is `CONNECTION_CHANGE` and the device has an autoconnect status.

#### Scenario: Pairing completes for a device with an autoconnect status
- **WHEN** a `PAIRING_CHANGE` notification is sent for a device that has an autoconnect status
- **THEN** the notification has no `autoconnect` key

### Requirement: Unpair result
`unpair` SHALL report `success` as the outcome of the unpair, and `success:false` when `deviceID` is missing or malformed.

#### Scenario: Unpair fails
- **WHEN** the backend fails to unpair the device
- **THEN** the response contains `"success":false`

#### Scenario: Missing or malformed device ID
- **WHEN** `unpair` is called without `deviceID`, or with a `deviceID` that is not a number
- **THEN** the backend is not called and the response contains `"success":false`

#### Scenario: Unknown device on the SDK backend
- **WHEN** `unpair` is called for a device the SDK backend does not know
- **THEN** the response contains `"success":false`, as on the BTMgr backend

### Requirement: Autoconnect device ID matches the stored key
`setAutoConnect`, `getAutoConnect` and the `AUTOCONNECT_STATUS_CHANGE` notification SHALL convert a numeric `deviceID` string to canonical decimal form before looking up the paired-device entry, and SHALL pass any other string through unchanged. This is a deliberate difference from the legacy plugin, which used the string as sent.

#### Scenario: Non-canonical numeric device ID
- **WHEN** a client calls `getAutoConnect` with `"0123"` and the paired device is stored as `"123"`
- **THEN** the device is found

#### Scenario: Non-numeric device ID
- **WHEN** a client calls `getAutoConnect` with a MAC address that matches no stored entry
- **THEN** the response contains `"success":false`

### Requirement: Audio info on failure
`getAudioInfo` SHALL report `success:true` with an empty `trackInfo` object when the track information cannot be read, and the populated object otherwise.

#### Scenario: Track info unavailable
- **WHEN** the backend cannot provide track information
- **THEN** the response contains `"trackInfo":{}` and `"success":true`

### Requirement: Volume and mute operation type
For `getDeviceVolumeMuteInfo` and `setDeviceVolumeMuteInfo` on the BTMgr backend, the `deviceType` SHALL map to the BTMgr operation type as the legacy plugin did: audio output for loudspeaker, headphone, wearable headset or hi-fi audio names, audio input for smartphone or tablet, HID for keyboard, mouse or joystick, LE for LE names, unknown for `DEFAULT`, and audio output for anything else. There SHALL be no audio plus HID combination.

#### Scenario: Unrecognised device type
- **WHEN** the device type is not recognised
- **THEN** the volume and mute operation uses the audio output type

### Requirement: Payload audit against legacy
Every notification and response payload produced by the plugin SHALL be compared field-by-field with `rdk_develop_branch` for key names, JSON types and presence rules, and each difference SHALL be fixed or recorded in the SDK gap report or a follow-up change.

#### Scenario: Audit complete
- **WHEN** the audit finishes
- **THEN** every difference is either fixed with a test or listed with its reason in a written record

### Requirement: SDK gap report
A gap report SHALL be maintained for the SDK team listing each client-visible behavior that cannot reach legacy parity without SDK changes, with the legacy behavior, supporting `file:line` evidence, the SDK's current surface, and the requested capability.

#### Scenario: Gap report content
- **WHEN** the report is reviewed
- **THEN** each entry has legacy behavior, evidence, current SDK surface and requested API
