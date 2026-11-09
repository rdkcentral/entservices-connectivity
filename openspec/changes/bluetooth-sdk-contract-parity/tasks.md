## 1. New-track payload typing

- [x] 1.1 In `Bluetooth/Bluetooth.cpp` (`onNewTrack` callback), emit `ui32Duration`, `ui32TrackNumber` and `ui32NumberOfTracks` with `std::to_string`
- [x] 1.2 Extend the BTMgr L2 test `BluetoothOnPlaybackNewTrack` to assert the three fields are JSON strings (L1 mocks the adapter and cannot observe notifications; the callback is shared by both backends)

## 2. Active state

- [x] 2.1 In `BtSdkAdapterImpl::deviceToInfo`, set `powerStatus` to 1 when the device is connected, else 0; replace the "known gap" comment
- [x] 2.2 Add an SDK L2 test asserting `powerStatus` 1 for a connected device, and an L1 test asserting the `getConnectedDevices` JSON carries `"activeState":"1"`

## 3. RSSI and signal level

- [x] 3.1 Widen `IBtAdapter::BtDeviceProperties::rssi` to `int32_t` and update `BtMgrAdapterImpl` (remove the `int16_t` cast) and the mocks
- [x] 3.2 (`Bluetooth/SignalLevel.h`) Add a signal-level helper with the legacy thresholds (>= -60 → 4, >= -70 → 3, >= -90 → 2, else 1) in a backend-independent location
- [x] 3.3 In `BtSdkAdapterImpl::getDeviceProperties`, select the paired branch from `DeviceProperties::paired` (fall back to `DeviceState` Paired or Connected when absent): paired → `rssi = 0`, `signalLevel = 0`; unpaired → `rssi` = raw value or `INT_MIN` when absent, `signalLevel` = helper result; replace the "known gap" comment
- [x] 3.4 Add L1 unit tests for the helper boundaries (-60, -61, -70, -71, -90, -91, `INT_MIN`)
- [x] 3.5 Add SDK L2 tests for `getDeviceProperties` (paired with a known RSSI, paired flag absent, unpaired with RSSI, unpaired without RSSI, connected-but-unpaired) and L1 tests for the `getDeviceInfo` JSON strings
- [ ] 3.6 Verify on a device that BTMgr `getDeviceInfo` for a paired device returns `rssi` 0 and `signalStrength` 0 (baseline A), and that an unpaired scanned device returns the raw value and mapped level

## 4. Adapter name

- [x] 4.1 Implement `BtSdkAdapterImpl::setAdapterName` with `Adapter::setName`, truncating to 63 bytes, returning false on SDK failure; update the comment (leave `getAdapterName` unsupported)
- [x] 4.2 Replace the SDK L2 "set/get name unsupported" test with tests for `getName` unsupported, `setName` success, an overlong name and SDK failure (adds `setName` to the SDK test mocks)

## 5. Payload audit

- [x] 5.1 Compare every notification in `rdk_develop_branch` `notifyEventWrapper` with the current plugin's event path: key names, JSON types, presence rules (for example `onPlaybackChange` `ended` omits position and duration) (see `audit.md`)
- [x] 5.2 Compare every method response (`getPairedDevices`, `getConnectedDevices`, `getDiscoveredDevices`, `getDeviceInfo`, `getAudioInfo`, `getDeviceVolumeMuteInfo`, `getProperties` and the rest) with legacy (see `audit.md`)
- [x] 5.3 Fix each difference with a test, or record it in the gap report or a follow-up change with a reason. Fixed with tests: `onRequestFailed` `lastConnectedState`, `unpair` result, SDK `unpair` of an unknown device, `getAudioInfo` on failure, `autoconnect` only on `CONNECTION_CHANGE`, volume/mute operation type. Kept by decision and recorded in `audit.md`: SDK power-status events, SDK `startScan` failure status, numeric `deviceID` conversion in the autoconnect wrappers (`toCacheKey`)

## 6. QA guide and SDK gap report

- [x] 6.1 Update `docs/bluetooth_backend_qa_test_guide.md` section 3 (and the matching rows in `docs/bluetooth_backend_developer_test_guide.md`): `onPlaybackPosition` → `onPlaybackProgress`; `activeState` appears only in `getConnectedDevices` and is now at parity; `signalStrength` is real only for unpaired devices and paired devices report 0 on both stacks; `setName` works while `getName` remains unsupported
- [x] 6.2 Review `docs/bluetooth_sdk_gap_report.md` with the SDK team and record the outcome of each item (outcomes recorded in the report, 2026-11-09)
- [ ] 6.3 Build and run the existing Bluetooth L1 tests and confirm they pass
- [ ] 6.4 Add an SDK adapter L1 or L2 test that a failed `pair` and a failed `connect` return failure with no `onRequestFailed` notification, and confirm the SDK adapter never calls the asynchronous `pair(false)` / `connect(false)`

## 7. One connected audio output device

- [ ] 7.1 Check that `AuthBridge::isAudioOutputType` (`HEADPHONES`, `LOUDSPEAKER`, `WEARABLE HEADSET`, `HANDSFREE`, `HIFI AUDIO DEVICE`) matches the devices btrCore classifies as `enBTRCoreSpeakers` or `enBTRCoreHeadSet` (`bluetooth/src/btrCore.c:837-905`), and record any difference
- [ ] 7.2 In `BtSdkAdapterImpl::connectDevice`, when the target is an audio output type, return success if it is already connected; otherwise disconnect every other connected audio output device synchronously, failing the connect if any disconnect fails, then connect. Smartphones and tablets neither trigger nor are hit by the rule
- [ ] 7.3 Start the new device's `beginDeviceOperation` before the old-device disconnects and keep the disconnects outside any held-status scope of their own, so the old device's `CONNECTION_CHANGE` is not held
- [ ] 7.4 Add SDK adapter L1 tests: other audio device disconnected first; disconnect failure aborts the connect; only a smartphone connected leaves it alone; smartphone connect leaves the audio device alone; target already connected
- [ ] 7.5 Test the notification order (old device change, connect response, new device change) at the `EventBridge` level if possible, otherwise verify it on a device against BTMgr
- [ ] 7.6 Update `docs/bluetooth_sdk_gap_report.md` item 5 and the QA and developer test guides: the connected-list filter is replaced by disconnect-first on connect, and externally connected devices are not covered
