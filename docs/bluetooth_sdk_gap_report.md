# Bluetooth SDK gap report: parity with the BTMgr-based plugin

**Audience:** Bluetooth SDK team
**Purpose:** Discussion document. It lists client-visible behavior of the `org.rdk.Bluetooth` plugin that the SDK-based backend cannot reproduce without SDK changes.
**Source of truth:** the BTMgr-based plugin on `rdk_develop_branch` (`entservices-connectivity`, `bluetooth_mgr`, `bluetooth`). The requirement is that clients see no difference between the BTMgr stack and the SDK stack, so the legacy code, not documentation, defines the expected behavior.
**SDK baseline reviewed:** `bluetooth-sdk` `develop` at `f9bd75a` (2026-11-03).
**Status:** reviewed with the SDK team on 2026-11-09. The outcome of each item is in the summary table and at the top of each section. The original request and evidence are kept below for the record.

Evidence paths: `P` = `rdk_develop_branch/entservices-connectivity/Bluetooth/Bluetooth.cpp`, `M` = `bluetooth_mgr/src/ifce/btrMgr.c`, `C` = `bluetooth/src` (btrCore), `S` = `bluetooth-sdk`.

## Summary

| # | Capability needed | Legacy API affected | SDK today | Outcome (2026-11-09) |
|---|---|---|---|---|
| 1 | Adapter name getter | `getName` | `Adapter::setName` only | **SDK will add** a generic (not BlueZ-specific) getter |
| 2 | Adapter discoverable get/set with timeout | `isDiscoverable`, `setDiscoverable` | not exposed | **Not needed.** Only for audio-in (stream-in); LE advertising covers current devices |
| 3 | AVRCP media player: controls, track info, state events | `sendAudioPlaybackCommand`, `getAudioInfo`, `onPlaybackChange`, `onPlaybackProgress`, `onPlaybackNewTrack` | none | **Not needed.** Only for stream-in; the SDK will not use the media player object |
| 4 | A2DP transport state ("playing") | `onPlaybackRequest` | none | **Not needed.** Stream-in only; if it returns it will probably use HFP or HSP |
| 5 | "Currently streaming" audio device | `getConnectedDevices` | none | **Plugin.** Connecting an audio output device first disconnects any other, so the connected audio output device is the streaming one |
| 6 | Audio volume/mute parity and events | `getDeviceVolumeMuteInfo`, `setDeviceVolumeMuteInfo`, `onDeviceMediaStatus` | `AUDIO_SUPPORT` | **Answered.** SDK covers the AVRCP absolute volume path and events; the plugin implements volume and mute with BTMgr's persisted volume and immediate `onDeviceMediaStatus` (`AudioBridge`) |
| 7 | Primary/auxiliary audio stream selection | `setAudioStream` | none | **Not needed now.** The SDK is fixed to PRIMARY; auxiliary can be added if required |
| 8 | Device-type-specific connect semantics | `connect` / `disconnect` | generic `Device::connect` | **Not needed.** A plain connect works for A2DP source; sink (stream-in) is not enabled |
| 9 | Failure events | `onRequestFailed` | no failure `DeviceEvent`s | **Closed.** The plugin uses synchronous `pair`/`connect`; failure is the response, with no `onRequestFailed` on the SDK stack |

Items 1 to 9 were what the plugin team could not close alone. Items resolvable in the plugin (payload typing, `activeState`, `rssi` and `signalStrength`, `setName` wiring) are tracked in the `bluetooth-sdk-contract-parity` change and need nothing from the SDK team.

---

## 1. Adapter name getter

**Outcome:** the SDK team will add a getter. It must be generic, not BlueZ-specific. The plugin wires it in when it lands.

- **Legacy:** `getName` calls `BTRMGR_GetAdapterName` (`P:795`), which reads the adapter property through btrCore (`C: BTRCore_GetAdapterName`). The key used is `Alias` (`C: btrCore_dbus_bluez5.c:3742`, `:4099`). `setName` uses the same property (`P:775`).
- **SDK today:** `Adapter::setName(const std::string&)` writes `Alias` (`S: Adapter.cpp:372`). There is no getter. The generated BlueZ proxy already exposes `Alias()`.
- **Request:** `Status Adapter::getName(std::string& name)` returning `Alias`.
- **Notes:** the plugin wires `setName` regardless. Until this lands, a set then read-back round trip fails on the SDK stack.

## 2. Adapter discoverable get/set with timeout

**Outcome:** not needed. Pairable is already set inside the SDK `pair` for every device type. Discoverable matters only for audio-in; the current LE devices use advertisement. Accepted difference: both calls stay unsupported.

- **Legacy:** `isDiscoverable` calls `BTRMGR_IsAdapterDiscoverable` (`P:354`, `M:4600`); `setDiscoverable` calls `BTRMGR_SetAdapterDiscoverable(adapter, enable, timeout)` (`P:752`, `M:4536`).
- **SDK today:** BlueZ `Adapter1.Discoverable` and `DiscoverableTimeout` exist in the generated proxy but the public `Adapter` class exposes neither. The `Discoverable` in `Advertisement.h` is the LE advertisement flag and is unrelated.
- **Request:** `getDiscoverable(bool&)` and `setDiscoverable(bool, uint32_t timeoutSeconds)`.
- **Questions for the SDK team:**
  - How does this interact with the `Pairable` handling the SDK does internally during pairing? A phone pairing in needs the adapter pairable as well as discoverable.
  - BlueZ treats timeout `0` as "no timeout". The plugin passes the client's value through unchanged, as legacy does; confirm the SDK keeps that semantics.

## 3. AVRCP media player: controls, track info, state events

**Outcome:** not needed. These serve audio-in (stream-in) only, and the SDK will not use the media player object. Accepted difference: the media controls, track info and playback notifications are not provided.

- **Legacy behavior (this is a state machine, not a signal forward):**
  - Controls: `PAUSE`, `RESUME` (play), `STOP`, `SKIP_NEXT`, `SKIP_PREV` map to `BTRMGR_MediaControl` (`P:822`-`834`). `RESTART` always fails.
  - Track info: `getAudioInfo` returns album, genre, title, artist, duration, track number, number of tracks (`P:1013`, `M:7810`).
  - Events: `onPlaybackChange` actions `started`, `paused`, `stopped`, `ended`; `onPlaybackProgress`; `onPlaybackNewTrack` (`P:1257`-`1315`).
  - Source: `C: btrCore_avMedia.c` `btrCore_AVMedia_PlaybackPositionPolling` (around line 782) polls the BlueZ `MediaPlayer1.Position` property once per second while playing and every 100 ms otherwise. `started` is emitted on the first poll after a track change. `ended` is emitted when the player object disappears. A pause is emitted only when the state changes.
- **SDK today:** none. `specs/` contains only `org.bluez.Media1`, with no `MediaPlayer1`. `AUDIO_SUPPORT` covers volume, mute and delay only.
- **Request:** a media player wrapper over `org.bluez.MediaPlayer1` with:
  - controls: play, pause, stop, next, previous (and fast-forward, rewind for completeness);
  - properties: `Status`, `Position`, `Track` (Title, Artist, Album, Genre, Duration, TrackNumber, NumberOfTracks);
  - a way to observe player appearance and disappearance per device, and property changes.
- **Open design question:** whether the position polling and the started/paused/ended derivation live in the SDK or in the plugin. The plugin team is happy to own the state machine if the SDK provides the primitives above.
- **Notes:** the L3 `avrcp` test covers inbound passthrough keys from a peer. That is the opposite direction from the controls requested here.

## 4. A2DP transport state ("playing")

**Outcome:** not needed. Stream-in is out of scope; if it is wanted later it will probably use the HFP or HSP profile. Note for that case: WirePlumber has no `a2dp_sink` role configured today.

- **Legacy:** for a smartphone or tablet, `onPlaybackRequest` is raised when btrCore reports the device state `enBTRCoreDevStPlaying` (`M:10049`). The client may answer through `respondToEvent` for that event type; an accepted answer makes BTMgr call `BTRMGR_StartAudioStreamingIn` for the device (`M:6887`-`6893`), so the SDK side needs a way to start the phone-to-STB audio path too (see item 8).
- **SDK today:** no way to observe the A2DP transport state (`org.bluez.MediaTransport1.State`).
- **Request:** a per-device transport state accessor and change event (idle, pending, active).

## 5. "Currently streaming" audio device

**Outcome:** plugin work. With PipeWire the transport is `idle` after connect and `active` only while audio plays, so the transport state cannot identify the streaming device. Instead the plugin keeps one connected audio output device by disconnecting any other before it connects a new one, as BTMgr does in `btrMgr_StartAudioStreamingOut`. See "One connected audio output device" in `bluetooth-sdk-contract-parity`. Smartphones and tablets do not count. Devices that connect from their own side are not covered yet.

- **Legacy:** `BTRMGR_GetConnectedDevices` skips any connected audio-output device that is not the one currently streaming (`M:5797`-`5806`), so only that device is listed. The "streaming" device is the one on which `StartAudioStreamingOut` ran.
- **SDK today:** the plugin lists every device in the `Connected` state.
- **Request:** a way to learn which connected audio-output device is the active sink. This depends on how audio routing is solved (items 6 and 7); it may be derivable from the PipeWire route state.

## 6. Audio volume and mute

**Outcome:** answered. (1) A Route volume change reaches the speaker as AVRCP absolute volume when the remote supports it; the bluez5 SPA plugin handles that. The plugin reproduces BTMgr's persisted volume and its immediate `onDeviceMediaStatus` after a set (`AudioBridge`). (2) Device-initiated changes arrive as `AudioEvent::VolumeChanged`; mute is handled in PipeWire and the plugin relies on `MuteStateChanged`. (3) `initAudio` is covered at L3 only, since it needs the real WirePlumber node. (4) `AUDIO_SUPPORT` is enabled wherever the SDK is built.

- **Legacy (`M:7577`, `BTRMGR_SetDeviceVolumeMute`):**
  - Scale is 0 to 255.
  - For output devices it first tries AVRCP absolute volume. If that succeeds it pins the local stream volume to maximum; otherwise it sets the local stream volume. Mute is always local.
  - After every set it posts `onDeviceMediaStatus` with a `command` derived by comparing with the previous state: mute change, then volume up or down, else unknown.
  - `VOLUME_UP` and `VOLUME_DOWN` try AVRCP first, then fall back to local steps of 10 (minimum 5 going up, maximum 250 going down) (`M:7040`-`7124`).
  - Volume is persisted per device and profile when `RDKTV_PERSIST_VOLUME` is defined.
  - Device-initiated volume changes also raise `onDeviceMediaStatus` (`M:10877`-`10953`).
- **SDK today:** `Device::setVolume/getVolume/setMute/isMuted/setDelayCompensation` and `registerForAudioEvents` behind `AUDIO_SUPPORT`, using the PipeWire Route volume (float 0.0 to 1.0). The option defaults to OFF and needs WirePlumber.
- **Questions for the SDK team:**
  1. Does a Route volume change reach the speaker as AVRCP absolute volume (for example through WirePlumber's Bluetooth hardware-volume support), or only change the local sink? The plugin needs to know whether the SDK path covers both legacy branches.
  2. Is a device-initiated volume change (headset buttons) reliably delivered through `AudioEvent::VolumeChanged` and `MuteStateChanged`?
  3. L2 states that automatic `Manager` to `Device` audio binding (`initAudio`) is not covered. Is it covered by L3 on hardware?
  4. Is the `AUDIO_SUPPORT` build expected to be the default for product images?

## 7. Primary or auxiliary audio stream

**Outcome:** not needed now. The capture type is fixed to PRIMARY. A platform can use auxiliary only through the audio capture binary's service file, with no runtime switch, and the Amlogic HAL has no auxiliary read path. Broadcom and Realtek have one, so the SDK can add it if needed.

- **Legacy:** `setAudioStream` calls `BTRMGR_SetAudioStreamingOutType` with PRIMARY or AUXILIARY (`P:689`). This selects which STB audio stream feeds the Bluetooth output.
- **SDK today:** nothing equivalent. Audio routing is owned by PipeWire and WirePlumber.
- **Request:** a design discussion. Possible answers: a PipeWire routing contract the plugin drives, an SDK API, or an explicit platform statement that the selection is handled elsewhere. Which streams exist on SDK-based products also needs confirming.

## 8. Device-type-specific connect semantics

**Outcome:** not needed. For audio output devices a plain connect works and PipeWire/WirePlumber handle A2DP source routing. A2DP sink (stream-in) is not enabled, so the phone-to-STB path does not exist; it is needed only where `--enable-streaming-in` is set.

- **Legacy `setDeviceConnection` (`P:627`-`680`):** LE tile and HID devices use `BTRMGR_ConnectToDevice` with the matching operation type. Smartphones and tablets call `BTRMGR_StartAudioStreamingIn` (audio from the phone into the STB). All other devices call `BTRMGR_StartAudioStreamingOut`.
- **SDK today:** `Device::connect` is generic. The plugin ignores the device type on the SDK path.
- **Question:** does a plain connect result in working A2DP sink (phone to STB) and source (STB to speaker) routing on the target stack, with no SDK or plugin action? If not, what is the expected sequence?

## 9. Failure events

**Outcome:** closed. The plugin calls only the synchronous `pair(true)` and `connect(true)`, and the SDK team confirmed every pairing or connection problem is then reported through the return `Status`. The client uses the response (`success:false`) as the failure signal. On the SDK stack there is no `onRequestFailed` for a failed pair or connect; BTMgr still sends one. A failure from the asynchronous calls would not be visible, so the plugin must never use them.

- **Legacy:** BTMgr posts `BTRMGR_EVENT_DEVICE_PAIRING_FAILED` and `BTRMGR_EVENT_DEVICE_CONNECTION_FAILED`, which the plugin surfaces as `onRequestFailed` (`P:1145`-`1187`).
- **SDK today:** the SDK emits no failure `DeviceEvent` (tracked as INV-3 in `btmgr-to-sdk-refactor`). The plugin falls back to checking the synchronous `Status`.
- **Request:** `PairFailed` and `ConnectFailed` style events, or confirmation that the synchronous `Status` is the intended contract. Timing relative to the call's response matters for QA's notification-order checks.

---

## Informational (no action requested)

- `Device::signalStrength()` shells out to `btmgmt conn-info` and uses thresholds (-55, -65, -75, -85, with a `None` level) that differ from the legacy mapping (-60, -70, -90, minimum level 1). The plugin will not use it and instead maps the `RSSI` property itself. Calling `popen` from a library may be worth reviewing independently.
- Gamepad connection rules (unpaired gamepad rejection, the last-disconnected hold-off for four specific models, and the HID gamepad RFC gate) are plugin-side work and need nothing from the SDK beyond the existing `modalias` properties.

## Next steps

1. SDK team adds the generic adapter name getter (item 1); the plugin then wires it in.
2. Plugin team implements disconnect-first on connect (item 5) and the volume persistence behavior (item 6).
3. Plugin team records the accepted differences (items 2, 3, 4, 9) in the QA and developer test guides.
4. Revisit items 2 to 4, 7 and 8 if audio-in (stream-in) is ever enabled on a product.
