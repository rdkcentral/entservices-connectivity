# Payload audit against `rdk_develop_branch`

Method: field-by-field comparison of notification and response payloads (key names, JSON types, presence rules, values) between the legacy `Bluetooth.cpp` and the current plugin, followed by a manual check of every candidate against both code locations. Nothing was run on a device.

## Fixed in this change

| Finding | Backend | Fix |
|---|---|---|
| `onRequestFailed` lacked `lastConnectedState` | BTMgr | Added to the callback and payload |
| `unpair` always reported `success:true`; the code comment wrongly called this legacy alignment | both | Reports the real result, `false` for a missing or malformed `deviceID` |
| `getAudioInfo` returned a populated object of empty strings on failure | BTMgr failure, SDK always | `valid` flag; empty `trackInfo` object on failure |
| `PAIRING_CHANGE` carried `autoconnect` | both | `autoconnect` only on `CONNECTION_CHANGE` |
| Volume and mute used the scan-profile mapping (default unknown, audio plus HID combination) | BTMgr | Own mapping as legacy: default audio output, no combination |
| `onPlaybackNewTrack` numeric fields typed as numbers | both | Strings (earlier task) |
| `unpair` of a device the SDK does not know reported success (decision: fail as BTMgr does) | SDK | Fails, as `BTRMGR_UnpairDevice` does ("Not a Paired device") |
| `activeState`, `rssi`, `signalStrength`, `setName` | SDK / both | Earlier tasks |

## Decided to keep

| Finding | Decision |
|---|---|
| `HARDWARE_AVAILABLE` and `SOFTWARE_DISABLED` `onStatusChanged` events on the SDK backend (BTMgr has no adapter power event) | Leave as is. This is a deliberate difference on the SDK backend. |
| `startScan` `status` on a scan start failure (legacy answers `AVAILABLE` and treats the scan as running; the SDK backend answers `NO_BLUETOOTH_HARDWARE`) | Keep current. This is a deliberate difference on the SDK backend. |
| `setAutoConnect` / `getAutoConnect` convert a numeric `deviceID` to canonical decimal (`toCacheKey`, added in `bbbceb0` as a scanner cleanup); legacy used the string as sent | Keep current. This is a deliberate difference on both backends. It was briefly removed and then restored; the original scanner finding is not recorded. |

## SDK data limitations (not plugin defects)

- `lastConnectedState` is always `false` in SDK status events (`EventBridge.cpp`). It needs plugin-side tracking of the last connected device, not an SDK change.
- `onPairingRequest` and `onConnectionRequest` send `supportedProfile:""`, `pinRequired` false, and `manufacturer:"0"` (`AuthBridge.cpp:134,221`).
- `getDeviceInfo` `supportedProfile` carries UUIDs where BTMgr gives profile names; `manufacturer` is the first manufacturer-data key.
- `deviceType` comes from `DeviceTypeClassifier`, a subset of BTMgr's strings.
- `respondToEvent` ignores `eventType`.
- `getName` fails with `name:""`; discoverable is unsupported; audio and playback events are absent. These are in the SDK gap report.

## Not verified

- Whether BTMgr emits `DEVICE_FOUND` for the same devices the SDK maps to `onDeviceFound`, and whether the discovered-devices list matches.
- Event ordering and timing on the SDK backend, and the 40 second auto-connect behavior.
- The `BTRMGR_SetDeviceVolumeMute` rejection path for a non-audio-output type, and `mute` values other than 0 and 1.
- The values BTMgr puts in `m_isLastConnectedDevice` and `m_powerStatus` on a live device.
- Persistence-migration responses.
