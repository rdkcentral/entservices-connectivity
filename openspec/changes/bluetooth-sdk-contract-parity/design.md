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
- Audio, playback and volume behavior, gamepad connection rules, and the connected-list "currently streaming" filter. Those need SDK input and are tracked in the gap report.
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

## Open Questions

- Whether the BTMgr connected-list filter (only the currently streaming audio-output device is listed) should be handled here. Current view: no, it needs a streaming notion tied to audio routing, so it goes in the gap report.
