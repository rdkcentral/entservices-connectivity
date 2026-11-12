## Why

The Bluetooth plugin is moving from the BTMgr stack to the Bluetooth SDK stack, and clients must see no difference. The source of truth is the legacy implementation on `rdk_develop_branch`, not documentation. A code-level comparison found places where the plugin's own contract (JSON payloads and field values) differs from legacy, independent of what the SDK can do. Some affect the BTMgr backend as well, and others only the SDK backend. They can be fixed inside this repository with no SDK changes.

## What Changes

- `onPlaybackNewTrack` emits `ui32Duration`, `ui32TrackNumber` and `ui32NumberOfTracks` as strings, as legacy does (shared path, both backends).
- `activeState` in `getConnectedDevices` reports `"1"` for every connected device on the SDK backend, as legacy does.
- `getDeviceInfo` `rssi` and `signalStrength` follow the legacy two-branch behavior exactly: paired devices report `0` and `0`; unpaired devices report the raw RSSI (`INT_MIN` when unknown) and a level mapped with the BTMgr thresholds.
- The internal `rssi` field is widened so the `INT_MIN` sentinel survives on both backends. Today the BTMgr adapter truncates it to `0`.
- `setName` on the SDK backend is wired to the SDK's `Adapter::setName`, which writes the same BlueZ `Alias` that BTMgr writes. The name is truncated to BTMgr's 63-character limit.
- An audit of all notification and response payloads against `rdk_develop_branch` for any further type, shape or ordering differences, with each finding fixed or recorded.
- The QA guide's known-differences table is corrected to match legacy behavior.
- A gap report is produced for the SDK team covering behavior that cannot be reproduced without SDK changes. This change owns the report; the SDK work itself is out of scope.

## Capabilities

### New Capabilities
- `bluetooth-legacy-contract-parity`: Requirements that the plugin's JSON-RPC responses and notifications, on both backends, match the `rdk_develop_branch` implementation in field names, value types, and value semantics for the covered fields.

### Modified Capabilities

## Impact

- Code: `Bluetooth/Bluetooth.cpp` (new-track callback), `Bluetooth/BtSdkAdapterImpl.cpp` (`deviceToInfo`, `getDeviceProperties`, `setAdapterName`), `Bluetooth/BtMgrAdapterImpl.cpp` (RSSI width), `Bluetooth/IBtAdapter.h` (RSSI field type).
- Tests: L1 tests for each of the above, including mock updates.
- Docs: `docs/bluetooth_backend_qa_test_guide.md` (section 3), new `docs/bluetooth_sdk_gap_report.md`.
- No SDK changes. No change to the public API surface.
- Related: `btmgr-to-sdk-refactor` (parent migration); this change closes parity gaps it did not cover.
