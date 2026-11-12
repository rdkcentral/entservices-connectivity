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
- [ ] 6.3 Build and run the existing Bluetooth L1 tests and confirm they pass (not possible here: the Thunder build is not available. The SDK adapter tests, 64 in all, pass in a local harness; the rest of the L1 and L2 suites still need your build)
- [x] 6.4 Add an SDK adapter L1 or L2 test that a failed `pair` and a failed `connect` return failure with no `onRequestFailed` notification, and confirm the SDK adapter never calls the asynchronous `pair(false)` / `connect(false)` (`pairDevice_SdkFailure_*`, `connectDevice_SdkFailure_*`, `pairAndConnect_Success_UseTheSynchronousCalls`; pass in the local harness)

## 7. One connected audio output device

- [x] 7.1 Checked `AuthBridge::isAudioOutputType` against btrCore's `enBTRCoreSpeakers` / `enBTRCoreHeadSet` (`bluetooth/src/btrCore.c:837-905`): btrCore treats every A2DP sink and HFP headset as one of the two, and `DeviceTypeClassifier` maps every audio/video class other than headphones, headset and hands-free to `LOUDSPEAKER` and an audio-sink UUID to `HEADPHONES`, so the sets match. `HIFI AUDIO DEVICE` in `isAudioOutputType` is never produced by the classifier, which is harmless. No difference found
- [x] 7.2 In `BtSdkAdapterImpl::connectDevice`, when the target is an audio output type, return success if it is already connected; otherwise disconnect every other connected audio output device synchronously, failing the connect if any disconnect fails, then connect. Smartphones and tablets neither trigger nor are hit by the rule (`disconnectOtherAudioOutputDevices`; the device's own resolved type decides, not the client's `deviceType`, which defaults to `SMARTPHONE` when omitted)
- [x] 7.3 Disconnect the other audio output devices before the new device's `beginDeviceOperation`, with no operation of their own, so the old device's `CONNECTION_CHANGE` is not held and a failed disconnect leaves nothing to release
- [x] 7.4 Add SDK adapter L1 tests: other audio device disconnected first; disconnect failure aborts the connect; only a smartphone connected leaves it alone; smartphone connect leaves the audio device alone; target already connected (written; not yet compiled or run, as the Thunder and GTest build is not available locally)
- [x] 7.5 Test the notification order at the adapter level (`connectDevice_AudioOutput_NotificationsFollowBtmgrOrder`: old device's change, connect response, new device's change); not yet compiled or run. Still to verify on a device against BTMgr
- [x] 7.6 Update `docs/bluetooth_sdk_gap_report.md` item 5 and the QA and developer test guides: the connected-list filter is replaced by disconnect-first on connect, and externally connected devices are not covered

## 8. Volume and mute on the SDK backend

- [x] 8.0 Build setup. Compared the stub's `Audio.h` and `Device.h` with `bluetooth-sdk` `develop` at `f9bd75a`: the audio declarations and `AudioEventData` layout are identical (only default member initializers and comments differ), so the symbols match. The stub always exports the audio symbols, because the plugin references them and fails to load without them (checked: the stub builds standalone and exports `setVolume`, `getVolume`, `setMute`, `isMuted`, `setDelayCompensation`, `registerForAudioEvents`). The audio API is no longer behind a build flag (`AUDIO_SUPPORT` / `BLUETOOTH_AUDIO_SUPPORT` were removed)
- [x] 8.1 Persistence is required: the BTMgr team and SDK team confirmed `RDKTV_PERSIST_VOLUME` is always enabled (2026-11-09), so the "Volume and mute persistence" requirement stands and the previous-state baseline is the stored value
- [x] 8.2 Move the device-type to operation-type mapping (`volumeOpTypeFromDeviceType` in `BtMgrAdapterImpl.cpp`) to a backend-independent helper and use it from `BtSdkAdapterImpl` (`VolumeUtils.h`, used by both backends)
- [x] 8.3 Add the 0 to 255 and 0.0 to 1.0 conversion as a small helper, with L1 tests for 0, 1, 100, 127, 128, 254 and 255 round-tripping (`volumeToSdk` / `volumeFromSdk`, round-trip tested for every value)
- [x] 8.4 Implement `BtSdkAdapterImpl::setDeviceVolumeMute` and `getDeviceVolumeMute` with `Device::setVolume/getVolume/setMute/isMuted`, with the connected, audio-output and invalid-mute checks (`AudioBridge::setVolumeMute` / `getVolumeMute`)
- [x] 8.5 Implement `setAudioControlCommand` for `VOLUME_UP`, `VOLUME_DOWN`, `AUDIO_MUTE` and `AUDIO_UNMUTE` with the local step rules; leave the other commands unsupported (`AudioBridge::sendCommand`)
- [x] 8.6 Register `registerForAudioEvents` for each connected audio output device and map `VolumeChanged` and `MuteStateChanged` to `onDeviceMediaStatus`, with the command derivation and the drop of the echo of the plugin's own changes (`AudioBridge::onAudioEvent`, echo dropped while the plugin makes its own change)
- [x] 8.7 Store the last volume per device and one mute state in `BluetoothDeviceManager` (a mute field is new), update them on every set, command and device change, and return them from `getDeviceVolumeMuteInfo` when present (`BluetoothDeviceManager`: stored flag per device, mute read from the store on first use; wired in `Bluetooth.cpp`). The manager changes have not been compiled or tested here: they need the Thunder build
- [x] 8.8 Apply the stored volume when an audio output device connects, once its audio is available, send one `onDeviceMediaStatus`, and suppress event notifications during the apply for up to 4 seconds (`AudioBridge::onDeviceConnected` marks the restore; the first SDK audio event after the connection applies it)
- [x] 8.9 Add the audio methods to the GMock `Tests/mocks/bluetooth/Device.h` (`setVolume`, `getVolume`, `setMute`, `isMuted`, `registerForAudioEvents`, as was done for `Adapter::setName`), then add SDK adapter L1 tests for each scenario of the four volume requirements, with the mock firing `VolumeChanged` and `MuteStateChanged` on demand (the mock `Device` and `MockDevice` have the audio methods; 25 new tests run and pass locally with a throwaway harness; no test yet for the manager's store, which needs the Thunder build)
- [ ] 8.10 Verify on a device against BTMgr: set, read back, volume commands, headset button changes, and volume after reconnect; the `AVRCP absolute volume` path needs a speaker that supports it
- [x] 8.11 Replace the audio rows in the QA and developer test guides (volume, mute and `onDeviceMediaStatus` now work on the SDK stack; media controls, track info, `setAudioStream` and the other playback notifications do not) (QA and developer guides)

- [x] 8.12 Make `setAudioStream` succeed for `PRIMARY` (the SDK's only stream, so nothing is changed) and fail for `AUXILIARY` and any other name on the SDK backend, with L1 tests for both (`BtSdkAdapterImpl::setAudioStream`)
