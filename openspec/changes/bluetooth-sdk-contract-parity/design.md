## Context

`rdk_develop_branch` (BTMgr-based plugin plus `bluetooth_mgr` / `bluetooth`) is the source of truth for client-visible behavior. The current plugin routes all operations through `IBtAdapter`, with `BtMgrAdapterImpl` and `BtSdkAdapterImpl` behind it, and `Bluetooth.cpp` formats the JSON. A line-by-line comparison against legacy found contract differences that need no SDK change:

- **New-track typing:** `Bluetooth.cpp` emits the three numeric track fields as JSON numbers; legacy emits strings.
- **`activeState`:** legacy `BTRMGR_GetConnectedDevices` sets `m_powerStatus = ACTIVE (1)` for every connected device. `deviceToInfo` never sets it on the SDK path.
- **RSSI and signal level:** legacy `BTRMGR_GetDeviceProperties` zero-initializes the result. A device found in the paired list returns early with `rssi = 0` and `signalLevel = 0`. Only a device found in the scanned list gets `rssi` (raw, `INT_MIN` when btrCore has no value) and `signalLevel = map(rssi)`. The SDK path currently reports BlueZ's raw RSSI for paired devices and level `0` for everything.
- **Adapter name:** BTMgr writes BlueZ `Alias` and caps the name at 63 bytes. The SDK's `Adapter::setName` writes `Alias`. The SDK path currently fails unconditionally.

## Goals / Non-Goals

**Goals:**
- Match legacy output exactly for the fields above on both backends.
- Keep the legacy-mapping logic in testable, backend-independent helpers where it is pure computation.
- Find any further payload differences by auditing against legacy, and fix or record them.
- Produce a gap report for the SDK team.

**Non-Goals:**
- Any SDK change (`getName`, discoverable, media, streaming state).
- Media controls, track info, playback notifications, delay compensation, `setAudioStream` and gamepad connection rules. The SDK team confirmed the media features serve stream-in only, which is out of scope. Volume and mute are handled by D9, and the connected-list "currently streaming" filter by D8.
- Improving legacy behavior. Where legacy looks odd (for example RSSI `0` for paired devices), it is reproduced.

## Decisions

### D1: Legacy code is the spec, including its quirks
RSSI `0` for paired devices and `-2147483648` for unknown RSSI on unpaired devices are reproduced. Rationale: the requirement is client-observable parity, and baseline A is what QA compares against. Alternative considered: report real BlueZ RSSI for paired devices. Rejected because it is a visible difference from baseline.

### D2: Paired decision uses the BlueZ `Paired` property
In `getDeviceProperties`, the paired branch is selected from `DeviceProperties::paired`, falling back to `DeviceState` Paired or Connected only if the property is absent. `DeviceState::Connected` alone is not used because it does not imply paired, while legacy sends a connected-but-unpaired device through the scanned branch.

### D3: Signal level mapping lives in one helper
`signalLevel = rssi >= -60 ? 4 : rssi >= -70 ? 3 : rssi >= -90 ? 2 : 1`, the legacy `btrMgr_MapSignalStrengthToRSSI` thresholds (`BTRMGR_SIGNAL_GOOD/FAIR/POOR`). Level `0` is never produced for an unpaired device. A missing RSSI is represented as `INT_MIN`, which maps to `1`.

### D4: Widen `IBtAdapter::rssi` to `int32_t`
`BtMgrAdapterImpl` currently casts BTMgr's `int m_rssi` to `int16_t`, which turns the `INT_MIN` sentinel into `0`. Widening fixes the BTMgr backend and carries the sentinel on the SDK backend. `signalLevel` stays small. Alternative considered: special-case the sentinel in a narrower type. Rejected as more fragile.

### D5: `activeState` is set in the SDK adapter, not in `Bluetooth.cpp`
`deviceToInfo` sets `powerStatus = 1` when connected, else `0`. Legacy only ever produces `ACTIVE` (1) for connected devices; `LOW` and `STANDBY` are never set. `Bluetooth.cpp` keeps formatting `std::to_string(info.powerStatus)` for both backends.

### D6: `setName` is wired now even though `getName` is not
The SDK has `Adapter::setName` (BlueZ `Alias`) and BTMgr writes the same property, so `setName` can reach parity now. `getName` still fails until the SDK adds a getter, so a set then read-back round trip stays incomplete. This is accepted because it is strictly closer to baseline, and the remaining gap is in the SDK gap report. The name is truncated to 63 bytes before the call.

### D7: Payload audit is a task, not a one-off check
A scripted heuristic found only the three new-track fields, but it cannot type variables. The audit compares every notification and response payload field-by-field against the legacy `notifyEventWrapper` and method handlers, covering JSON type, key name and presence rules (for example, `onPlaybackChange` `ended` omits position and duration).

## Risks / Trade-offs

- [Paired-device RSSI of 0 hides real data] → Intentional per D1; documented here and in the QA guide.
- [Misclassifying paired versus scanned changes which branch applies] → D2 uses BlueZ `Paired`; L1 tests cover paired, unpaired and connected-unpaired devices.
- [Widening `rssi` touches the shared interface and mocks] → Mechanical change; L1 compilation covers all call sites.
- [`setName` half-works until `getName` ships] → Called out in the gap report and the QA guide.
- [Audit finds additional differences that exceed this change] → Record them in the gap report or a follow-up change instead of widening scope.

### D8: Disconnect-first on connect replaces the connected-list filter
BTMgr lists only the streaming audio output device because `btrMgr_StartAudioStreamingOut` stops the streaming device and disconnects any other connected speaker or headset before it connects a new one, so at most one audio output device is connected. The SDK team confirmed the plugin should treat the connected audio output device as the streaming one and that the SDK needs no transport-state API for this. `BtSdkAdapterImpl::connectDevice` reproduces the disconnect-first rule, using the existing `isAudioOutputType` predicate, so `getConnectedDevices` needs no filter. Smartphones and tablets are not audio output devices here: stream-in is out of scope. Devices that connect from their own side are deferred. Event order follows BTMgr: the old device's disconnect runs inside the connect call and is not held, while the new device's change is held until after the response.

### D9: Volume and mute follow BTMgr's behavior over the SDK's audio API
The SDK team confirmed the audio path: a PipeWire Route volume change reaches the speaker as AVRCP absolute volume when the remote supports it, device-initiated changes arrive as `AudioEvent::VolumeChanged`, and mute is handled in PipeWire and reported as `MuteStateChanged`. So the plugin does what BTMgr did above that layer: it converts between 0 to 255 and 0.0 to 1.0, applies the local step for `VOLUME_UP` and `VOLUME_DOWN`, derives the `command` of `onDeviceMediaStatus` by comparison with the previous state, notifies at once instead of waiting for the device, and keeps the last volume and mute. It does not pin the stream volume to maximum, since BTMgr did that only because it owned the stream. The echo of a change the plugin made is dropped so a client sees one notification. `initAudio` binding is automatic in the SDK and is covered at L3 only, so the unit tests use the GMock `Device` in `Tests/mocks`. The SDK creates a device's audio object after the connection and gives no "audio ready" event, so a stored volume is applied when the first volume or mute change is reported for the device after it connects. The SDK sets and reports a change from inside its own setter, so the plugin marks its own change while it makes it and drops that echo. The SDK's getters report no error (volume 0 and unmuted when there is no audio), so a read is only invalid for a device that is not a connected audio output device. Persistence follows BTMgr's `RDKTV_PERSIST_VOLUME` behavior, which the BTMgr and SDK teams confirmed is always enabled. The plugin compiles against the in-tree stub and links the real SDK at runtime, so the stub's behavior is irrelevant and only its declarations matter (task 8.0).

### D10: `setAudioStream` accepts PRIMARY and refuses AUXILIARY
The SDK's capture stream is fixed to PRIMARY and cannot be switched at run time, and some platforms (Amlogic) have no auxiliary source. Accepting `PRIMARY` keeps a client that always sends it working, and refusing `AUXILIARY` avoids a call that appears to succeed but changes nothing. Auxiliary can be added later if a platform needs it.

## Open Questions

- Whether `isAudioOutputType` matches BTMgr's `enBTRCoreSpeakers` / `enBTRCoreHeadSet` classification exactly (task 7.1).
- Whether the notification order can be tested at the `EventBridge` level or only on a device (task 7.5).
