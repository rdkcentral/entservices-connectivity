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

### Requirement: One connected audio output device
Connecting an audio output device on the SDK backend SHALL first disconnect every other connected audio output device, as BTMgr's `btrMgr_StartAudioStreamingOut` does, so that at most one audio output device is connected. If any of those disconnects fails, the connect SHALL fail and SHALL NOT be attempted on the new device. A smartphone or tablet SHALL NOT count as an audio output device: connecting one SHALL NOT disconnect anything, and connecting an audio output device SHALL NOT disconnect one. Connecting a device that is already connected SHALL succeed, and SHALL disconnect only other connected audio output devices, if any. Devices that connect from their own side are out of scope.

The old device's disconnect SHALL run synchronously inside the connect call, outside the new device's held-status scope, so its `CONNECTION_CHANGE` reaches the client before the connect response, as on BTMgr. The new device's `CONNECTION_CHANGE` is still held until after the connect response.

#### Scenario: Another audio output device is connected
- **WHEN** a client connects headphones while a speaker is connected
- **THEN** the speaker is disconnected first, then the headphones are connected, and the connect reports success

#### Scenario: Disconnect of the other device fails
- **WHEN** the other audio output device cannot be disconnected
- **THEN** the connect reports failure and the new device is not connected

#### Scenario: Only a smartphone is connected
- **WHEN** a client connects headphones while only a smartphone is connected
- **THEN** the smartphone stays connected

#### Scenario: Connecting a smartphone
- **WHEN** a client connects a smartphone while an audio output device is connected
- **THEN** the audio output device stays connected

#### Scenario: Target already connected
- **WHEN** a client connects an audio output device that is already connected and no other audio output device is connected
- **THEN** the connect reports success and no device is disconnected

#### Scenario: Notification order
- **WHEN** a connect disconnects another audio output device first
- **THEN** the client receives the old device's `CONNECTION_CHANGE`, then the connect response, then the new device's `CONNECTION_CHANGE`

### Requirement: Pair and connect failure reporting
On the SDK backend, a failed `pair` or `connect` SHALL be reported only through the response (`success:false`), and the plugin SHALL NOT send `onRequestFailed` for it. The plugin SHALL call the SDK's synchronous `pair` and `connect` only, so every failure is visible in the returned `Status`. This is a deliberate difference from the BTMgr backend, which also sends `onRequestFailed`.

#### Scenario: Pairing fails on the SDK backend
- **WHEN** the SDK `pair` call returns a failure status
- **THEN** the response contains `"success":false` and no `onRequestFailed` notification is sent

#### Scenario: Connection fails on the SDK backend
- **WHEN** the SDK `connect` call returns a failure status
- **THEN** the response contains `"success":false` and no `onRequestFailed` notification is sent

#### Scenario: Pairing fails on the BTMgr backend
- **WHEN** BTMgr reports a pairing failure
- **THEN** the plugin sends `onRequestFailed` with `newStatus` `PAIRING_FAILED`, as before

### Requirement: Accepted differences from the SDK team review
The following differences between the SDK backend and legacy SHALL be accepted, with the SDK team's reasons recorded in the gap report: `getName` fails until the SDK adds a getter (the SDK team will add one); `isDiscoverable` and `setDiscoverable` are unsupported, and so are AVRCP media controls, track info, the A2DP playback request and the playback notifications, because those serve audio-in (stream-in) only and that feature is out of scope; `connect` is a plain SDK connect with no device-type-specific streaming call, since PipeWire and WirePlumber set up A2DP source routing.

#### Scenario: Gap report review recorded
- **WHEN** the gap report is reviewed
- **THEN** each item lists the SDK team's outcome and who owns any remaining work

### Requirement: SDK volume and mute get and set
On the SDK backend, `getDeviceVolumeMuteInfo` and `setDeviceVolumeMuteInfo` SHALL work for a connected audio output device through the SDK's `Device::getVolume/setVolume/isMuted/setMute`, as BTMgr's `BTRMGR_GetDeviceVolumeMute` and `BTRMGR_SetDeviceVolumeMute` do for audio output.

- The plugin volume is 0 to 255 and the SDK volume is a float from 0.0 to 1.0. The conversion SHALL be `sdk = plugin / 255` and `plugin = round(sdk * 255)`, so a set value reads back unchanged.
- The device operation type SHALL come from the same device-type mapping as the BTMgr backend. Only the audio output type is supported: any other type (smartphone, tablet, HID, LE), a device that is not connected, or a device with no SDK audio SHALL fail.
- `set` SHALL apply the volume and then the mute, and SHALL succeed only if both SDK calls succeed. A mute other than 0 or 1 SHALL fail.
- `get` SHALL report `valid` false for a device that is not a connected audio output device, so the response carries an empty `volumeinfo` as on BTMgr. The SDK reports no error for a read, so nothing else makes it invalid.
- AVRCP absolute volume is the SDK's job (the bluez5 SPA plugin in PipeWire). The plugin SHALL NOT pin the stream volume to maximum as BTMgr does.

#### Scenario: Set then get
- **WHEN** a client sets volume 100 and mute 0 on a connected speaker, then reads it back
- **THEN** the SDK volume is set to `100/255` and the read returns volume 100 and mute false

#### Scenario: Not an audio output device
- **WHEN** a client sets the volume of a smartphone
- **THEN** the call fails and the SDK is not called

#### Scenario: Device not connected
- **WHEN** a client sets the volume of a paired speaker that is not connected
- **THEN** the call fails

#### Scenario: SDK mute fails
- **WHEN** the SDK volume call succeeds and the SDK mute call fails
- **THEN** the call fails

#### Scenario: Not a connected audio output device
- **WHEN** a client reads the volume of a smartphone, or of a speaker that is not connected
- **THEN** `getDeviceVolumeMuteInfo` returns an empty `volumeinfo`

### Requirement: Volume and mute commands
On the SDK backend, `sendAudioPlaybackCommand` SHALL support `VOLUME_UP`, `VOLUME_DOWN`, `AUDIO_MUTE` and `AUDIO_UNMUTE` for a connected audio output device, and every other command SHALL keep failing as unsupported. BTMgr tries AVRCP first and falls back to local steps; with the SDK the AVRCP path is below the plugin, so the plugin SHALL apply the local step:

- `VOLUME_UP`: from a current volume below 5 the new volume is 5, from 5 to 245 it is the current volume plus 10, above 245 it is 255.
- `VOLUME_DOWN`: from a current volume above 250 the new volume is 250, from 10 to 250 it is the current volume minus 10, below 10 it is 0.
- Both volume commands report mute as true only when the new volume is 0.
- `AUDIO_MUTE` and `AUDIO_UNMUTE` set the mute and leave the volume unchanged.

#### Scenario: Volume up from a low volume
- **WHEN** the current volume is 3 and `VOLUME_UP` is sent
- **THEN** the volume becomes 5

#### Scenario: Volume down near zero
- **WHEN** the current volume is 7 and `VOLUME_DOWN` is sent
- **THEN** the volume becomes 0 and the reported mute is true

#### Scenario: Unsupported command
- **WHEN** `PAUSE` is sent
- **THEN** the call fails, as today

### Requirement: Device media status notification
On the SDK backend the plugin SHALL send `onDeviceMediaStatus` with `volume` (0 to 255), `mute` and `command` (`VOLUME_UP`, `VOLUME_DOWN`, `AUDIO_MUTE`, `AUDIO_UNMUTE` or unknown) for every successful volume or mute set, for every volume or mute command above, and for every change the device or the platform makes (`AudioEvent::VolumeChanged` and `MuteStateChanged`), so that the client updates without waiting for the device.

The `command` SHALL be derived as BTMgr's set does, by comparing with the previous state of that device: a mute change gives `AUDIO_MUTE` or `AUDIO_UNMUTE`; otherwise a higher volume gives `VOLUME_UP`, a lower one `VOLUME_DOWN`, and an equal one unknown. The previous state is the last value stored for the device (see the persistence requirement), starting from volume 254 and mute false.

A change the plugin made itself SHALL produce exactly one notification: the SDK event that echoes it SHALL be recognised and dropped.

#### Scenario: Volume set from the client
- **WHEN** a client raises the volume of a speaker
- **THEN** one `onDeviceMediaStatus` is sent with the new volume and `"command":"VOLUME_UP"`

#### Scenario: Mute set from the client
- **WHEN** a client mutes an unmuted speaker
- **THEN** one `onDeviceMediaStatus` is sent with `"mute":true` and `"command":"AUDIO_MUTE"`

#### Scenario: Volume changed with the headset buttons
- **WHEN** the SDK reports `AudioEvent::VolumeChanged` that the plugin did not cause
- **THEN** one `onDeviceMediaStatus` is sent with the new volume and a derived command

#### Scenario: SDK echo of the plugin's own change
- **WHEN** the SDK reports the volume change the plugin has just set
- **THEN** no second notification is sent

#### Scenario: Failed set
- **WHEN** a set fails
- **THEN** no notification is sent

### Requirement: Volume and mute persistence
On the SDK backend the plugin SHALL keep, as BTMgr does with `RDKTV_PERSIST_VOLUME` (always enabled), the last volume of each audio output device and one mute state for the box, in the plugin's own stored device data.

- A successful set, a volume or mute command and a device-initiated change SHALL update the stored values.
- `getDeviceVolumeMuteInfo` SHALL return the stored volume and mute when they exist, and the live SDK values otherwise.
- When an audio output device with a stored volume connects, the plugin SHALL apply that volume to the SDK device once its audio is available, which is when the SDK first reports a volume or mute change for it after the connection, and send one `onDeviceMediaStatus` with the stored volume and mute and a `command` derived from the device's own volume. That first change is not reported itself. The pending apply is dropped if the device disconnects first. SDK volume events caused by that apply SHALL NOT produce further notifications, and the suppression SHALL end after the apply is confirmed or after 4 seconds.

#### Scenario: Volume restored on connect
- **WHEN** a speaker with a stored volume of 120 connects
- **THEN** the SDK volume is set to `120/255` and one `onDeviceMediaStatus` with volume 120 is sent

#### Scenario: Nothing stored
- **WHEN** a speaker with no stored volume connects
- **THEN** the SDK volume is left as it is

#### Scenario: Stored value wins on read
- **WHEN** a client reads the volume of a device with a stored volume
- **THEN** the stored value is returned

### Requirement: Audio stream selection
On the SDK backend `setAudioStream` SHALL succeed for `PRIMARY` and fail for `AUXILIARY` and for any other name. The SDK's capture stream is fixed to PRIMARY and cannot be changed at run time, and not every platform has an auxiliary source, so `PRIMARY` reports what is already true and `AUXILIARY` is refused rather than silently ignored.

#### Scenario: Primary stream
- **WHEN** a client calls `setAudioStream` with `PRIMARY`
- **THEN** the response contains `"success":true`

#### Scenario: Auxiliary stream
- **WHEN** a client calls `setAudioStream` with `AUXILIARY`
- **THEN** the response contains `"success":false`

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
