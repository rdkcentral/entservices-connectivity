# Bluetooth: BTMgr vs. SDK Regression — QA Test Guide

**Audience:** QA / test engineers
**Purpose:** Explain *what* you are testing, *why*, and *how to approach it*. This is a **manual** test effort on a real device. Detailed test cases live in the existing test plan linked in [§5](#5-test-plan); this guide tells you how to use it and what to add.
**Baseline:** the existing Bluetooth behavior on `rdk_develop_branch`.

A developer-oriented companion with implementation detail is available in [bluetooth_backend_developer_test_guide.md](./bluetooth_backend_developer_test_guide.md). You do not need it to run this plan.

---

## 1. What changed and what you are testing

The Bluetooth service (`org.rdk.Bluetooth`) is being moved from the legacy **BTMgr** Bluetooth stack to the new **Bluetooth SDK** stack. Both are included in the same build, and a device setting decides which one is used.

**The promise to clients (IUI, apps, scripts) is that nothing changes:** same API methods, same parameters, same responses, same notifications, same device IDs, same behavior with real devices.

Your job is **regression testing**: show that the SDK-based Bluetooth behaves like the existing BTMgr-based Bluetooth, from the outside. You do this by running the same scenarios on each of the two stacks and comparing the results to the baseline.

> **Why this needs real testing:** BTMgr did a lot of things automatically that clients rely on (hiding certain devices during scans, deciding which incoming connections to accept, reconnecting the last speaker at boot, handling gamepads when the box sleeps). The SDK stack had to re-create all of that. Gaps show up as behavior differences, so those are the areas to look at hardest.

### Out of scope

- New features and anything not related to the BTMgr → SDK change.
- The paired-device *persistence migration* feature (`performMigration` / `clearMigration`). **Never call these during this testing.** If they are called by accident, restore the starting state as described in the existing test plan before continuing.

---

## 2. What "the same" means

For every scenario, compare these three things between the baseline and each stack:

1. **The API response** — success/failure and the returned fields (device lists, device IDs, device types, names, etc.).
2. **The notifications received** — which ones, with what values, **in what order**, and roughly when. In particular, a notification about an action (for example a pairing or connection change) must arrive **after** the response to that action.
3. **What actually happens with the device** — it pairs, connects, disconnects, appears or does not appear in scans, reconnects after reboot, and so on.

Internal log differences are **not** failures. Only differences a client or a user could notice are.

---

## 3. Known differences (expected — do not file as new bugs)

The SDK stack currently has these documented gaps. Confirm that they behave as described, and record them as **"known gap – confirmed."** Check with the dev team whether any have since been fixed.

| Feature | BTMgr stack | SDK stack (today) |
|---|---|---|
| Get/set the box's Bluetooth name (`getName`, `setName`) | Works | Fails (reported as unsupported) |
| Discoverable mode (`isDiscoverable`, `setDiscoverable`) | Works | Fails (reported as unsupported) |
| Audio control: `setAudioStream`, `sendAudioPlaybackCommand`, volume/mute (`getDeviceVolumeMuteInfo`, `setDeviceVolumeMuteInfo`), track info (`getAudioInfo`) | Works | Fails / returns nothing (audio not yet available in the SDK) |
| Playback/media notifications (`onPlaybackChange`, `onPlaybackPosition`, `onPlaybackNewTrack`, `onDeviceMediaStatus`, `onPlaybackRequest`) | Sent | Not sent |
| `activeState` value shown for devices | Real value | Always `0` |
| `signalStrength` in `getDeviceInfo` | Real value | Always `0` |
| Incoming connection from an **unpaired gamepad** | Handled by BTMgr's gamepad-specific rules | Treated like any other device (client is asked) |

Also, **do not file** these either:

- Anything that was already like that on the baseline (for example, the `RESTART` playback command failing).
- The "expected divergence" behaviors in the existing test plan, such as `setAutoConnect` being rejected and `getAutoConnect` reporting `false` when migration was never performed. These are the same on both stacks and are by design.

Everything else is expected to behave exactly like the baseline.

---

## 4. Before you start

### 4.1 Test set-up

- A device image that contains **both** stacks and a **real** Bluetooth SDK (not the placeholder/stub SDK).
- Shell access to the device and its logs.
- A way to call the Bluetooth API (curl to `http://127.0.0.1:9998/jsonrpc`) **and** a way to watch notifications as they arrive. Many checks are about notifications and their order.
- Confirm with dev which build options are in use (the existing test plan assumes the persistence-migration option is **on**) and that `performMigration` has never been called on the unit.

### 4.2 Test devices

Class matters because each class is handled differently. Use at least one of each if you can:

| Device | Why it is in the set |
|---|---|
| Headphones or speaker (and soundbar if available) | Device type naming, automatic reconnect at boot, short connection-request window |
| RDK remote control | Hidden from filtered scans, accepted automatically, must keep working while the box sleeps |
| Keyboard or other non-remote HID device | Accepted automatically, disconnected when the box sleeps |
| Gamepad (classic and/or low-energy) | Naming (`Game Controller`), special connection windows, sleep handling |
| Smartphone or tablet | Starts pairing/connection itself; the client is asked |
| Low-energy tag/tile | Device type and LE scan |

### 4.3 Selecting and confirming the stack

- **Select SDK:** set `BLUETOOTH_SDK_ENABLED=true` in `/etc/device.properties`, then restart the Bluetooth service (deactivate/activate).
- **Select BTMgr:** remove the setting (or set it to anything other than `true`), then restart the Bluetooth service.
- Ask dev for the approved way to switch a unit (including whether other services must be stopped). Never run both stacks at the same time.

> **Always confirm which stack is actually running.** If the SDK cannot start (for example, no adapter), the service silently falls back to BTMgr. A "SDK run" that is really BTMgr is a false pass.
>
> In the Bluetooth service log:
> - SDK running: you will see `BtSdkAdapterImpl::init: exit successfully`.
> - SDK requested but not running: you will see `BtAdapter::init: sdkError:` followed by an error message. **Stop and report this**; it is not a valid SDK run.
> - BTMgr running: no `BtSdkAdapterImpl` lines appear.
>
> Do this at the start of every run and after every reboot or service restart.

> On an image built **without** BTMgr, the SDK is always used, and the Bluetooth service will fail to start if the SDK cannot start (there is no fallback).

---

## 5. Test plan

### 5.1 Approach

Run each scenario three ways and compare:

| Run | What it is | Expectation |
|---|---|---|
| **A** | Baseline: `rdk_develop_branch` | Reference results (use recorded results if you already have them) |
| **B1** | This build, **BTMgr** selected | Must match A |
| **B2** | This build, **SDK** selected | Must match A, except for the known differences in [§3](#3-known-differences-expected--do-not-file-as-new-bugs) |

A simple result table per case works well: `Case | A | B1 | B2 | Match? | Known difference? | Notes`.

### 5.2 Part 1 — Core regression: run the existing plan on each stack

Use [bluetooth_pre_migration_regression_test_plan.md](./bluetooth_pre_migration_regression_test_plan.md) (cases `TC-PREREG-01` to `TC-PREREG-26`). It was written for exactly this situation: normal Bluetooth use with `performMigration` never called. **Run it on B1 and on B2**, and compare with A.

| Plan section | Covers | B1 (BTMgr) | B2 (SDK) | Notes for this effort |
|---|---|---|---|---|
| 1. Baseline / initial state | Boot state | Run | Run | Add the stack check from [§4.3](#43-selecting-and-confirming-the-stack) |
| 2. Discovery & scanning | Scan, stop, discovered list, scan notifications | Run | Run | Also repeat per scan profile (see Part 2) |
| 3. Pairing & unpairing | Pair/unpair | Run | Run | Watch notification order |
| 4. Connection management | Connect/disconnect | Run | Run | |
| 5. AutoConnect guard | Expected "divergence" for auto-connect | Run | Run | Same on both; not a bug |
| 6. Device info, audio & playback | Device info, audio, playback commands, responding to requests | Run | **Partly** | Audio items and `getAudioInfo` are known differences on SDK; `getDeviceInfo`'s `signalStrength` too. Responding to requests must pass |
| 7. Volume & mute | Volume/mute | Run | **Known difference** | Record as "known gap – confirmed" |
| 8. Adapter control | Enable/disable, discoverable, name | Run | **Partly** | `enable`/`disable` must pass; name and discoverable are known differences on SDK |
| 9. Gamepad / HID | Pairing and connecting HID/gamepads | Run | Run | Device type must read `HUMAN INTERFACE DEVICE` on both |
| 10. Power management | Behavior on power state changes | Run | Run | Also see Part 2 (gamepads and reconnect) |
| 11. No unwanted saved data | Nothing written to stored device data | Run | Run | |
| 12. Lifecycle / reboot | Restart service, reboot | Run | Run | |
| 13. Startup reconnect | Reconnect of last audio device at boot | **Check result only** | **Run** | On BTMgr just confirm the last speaker reconnects at boot and on power-on |

Apply the plan's exit checklist to each stack, reading "identical to a non-migration-capable build" as "identical to baseline A, apart from the known differences."

### 5.3 Part 2 — Extra checks for the areas most likely to differ

These focus on behavior that BTMgr performed on its own and the SDK stack had to reproduce. They are **not** covered by the existing plan. Run each on B1 and B2 and compare with A.

| ID | What to check | What to compare |
|---|---|---|
| SUP-01 | **Stack selection** | Setting `true` → SDK; setting absent, `false` or a nonsense value → BTMgr. If the SDK cannot start on a dual-stack image (for example, disconnect/disable the adapter or use a stub SDK), the service falls back to BTMgr and still starts. On an SDK-only image the service fails to start with a clear error |
| SUP-02 | **Switching stacks on one unit** | Pair devices on BTMgr, switch to SDK (and back), restart the service: paired devices are still listed with the **same device IDs**, and previously saved details (last connect time, auto-connect) are unaffected |
| SUP-03 | **Device IDs** | For each test device, the device ID is identical on both stacks and the baseline |
| SUP-04 | **Device types** | For every device in the set, compare the `deviceType` text in all device lists, `getDeviceInfo` and notifications. Pay attention to grouped types: soundbar/portable/car/hi-fi audio all show as `LOUDSPEAKER`; keyboard, remote and gamepad all show as `HUMAN INTERFACE DEVICE` |
| SUP-05 | **Scan by profile** | Run `startScan` with each profile (no profile, audio output, smartphone/tablet, HID, audio+HID, LE). Compare which devices are found. RDK remotes should appear only when no profile (or an unrecognized profile) is given. Also try different `timeout` values, including 0 or less, and `stopScan` |
| SUP-06 | **Names during discovery** | Use devices that advertise without a name (speaker, gamepad, phone). Compare the name shown and how long the device takes to appear. Unnamed gamepads should show as `Game Controller`. Expect devices to take up to a few seconds to appear on SDK |
| SUP-07 | **Notifications and their order** | For `pair`, `unpair`, `connect`, `disconnect`, `enable`, `disable`: record the response and every notification (`onStatusChanged` and its values, etc.). Notification must come **after** the response; nothing missing or duplicated. For scans: scan started/completed notifications, `onDiscoveredDevice`, `onDeviceFound`, `onDeviceLost` |
| SUP-08 | **Failures** | Pair/connect a device that is off, out of range or rejects; pair an already-paired device; connect an unpaired device; use an unknown device ID. Compare the failure response and the `onRequestFailed` notification |
| SUP-09 | **Requests started by the device** | Pair/connect *from* a phone/tablet, headphones, remote/keyboard, a classic gamepad and an LE gamepad. Compare: which request notification the client gets (`onPairingRequest` / `onConnectionRequest`), whether accept and reject (via `respondToEvent`) are honored, and what happens when nobody answers (it should be rejected after a short, device-specific wait). Judge by baseline A, not by an assumed time |
| SUP-10 | **Gamepads and sleep** | With a gamepad and a remote connected, put the box in STANDBY and in LIGHT_SLEEP: the gamepad disconnects and does not reconnect or show up in scans; the remote stays connected and still wakes the box. Return to ON: the gamepad can reconnect and be found again. DEEP_SLEEP and OFF should not by themselves change gamepad behavior |
| SUP-11 | **Reconnect at power-on** | With a speaker/headphones that was the last one used, (a) reboot and (b) wake from standby. It reconnects by itself on both stacks, in comparable time. If an audio device is already connected, nothing extra connects. A client rejecting the connection request keeps it disconnected |
| SUP-12 | **Enable/disable cycling** | Repeat `disable` / `enable` several times with devices paired. Lists and notifications stay consistent; no stale "connected" devices when the adapter is off |
| SUP-13 | **Service restart under load** | Deactivate/activate the Bluetooth service 10+ times, including while a scan runs or a request is waiting for an answer. No hangs or crashes; works after each restart; also after reboot |
| SUP-14 | **Overlapping actions** | Start two actions at once (for example `connect` while scanning), and connect from the client right after a startup reconnect request. No hangs, and no "In Progress" error where the baseline has none |
| SUP-15 | **Soak** | Run for several hours with periodic scan / pair / connect / disconnect cycles. Watch for memory growth, high CPU, or repeated errors in logs |

---

## 6. Judging results

**A case passes** when B1 and B2 results match baseline A in the response, notifications (including order) and real device behavior, apart from the known differences in [§3](#3-known-differences-expected--do-not-file-as-new-bugs).

**File a defect** when:

- **B1 differs from A** — the existing path changed.
- **B2 differs from A** and it is **not** a known difference — a gap in the SDK stack (examples: wrong device type text, a missing, duplicated or out-of-order notification, a device shown or hidden in a scan when it shouldn't be, a connection accepted/rejected differently, a reconnect or sleep behavior that doesn't match).
- A known difference is **worse than described** (for example, a crash instead of a failure response).
- The Bluetooth service fails to start, selects a different stack than configured, or something else is fighting over the Bluetooth adapter.

**In every defect, include:** which stack (BTMgr or SDK) and how you confirmed it, build information, the device and its class, the request and response, the notifications received (with times), and the relevant log excerpt.

**Do not file:** known differences ([§3](#3-known-differences-expected--do-not-file-as-new-bugs)); behavior that was already that way on the baseline; the expected "guard" results in the existing plan.

---

## 7. Sign-off checklist

- [ ] Active stack confirmed at the start of every run ([§4.3](#43-selecting-and-confirming-the-stack)).
- [ ] Existing regression plan completed on **both** stacks, results recorded as A / B1 / B2.
- [ ] SUP-01 to SUP-15 completed on both stacks (SUP-01 and SUP-02 as applicable to the image).
- [ ] Every B2 difference is either a known difference or a filed defect.
- [ ] No saved device data was written by the testing, and `performMigration` was never called.
- [ ] No crashes, hangs or unexpected errors on either stack.

---

## 8. Questions to settle with the dev team before starting

1. What is the approved procedure for switching a unit between BTMgr and SDK?
2. Are the known differences in [§3](#3-known-differences-expected--do-not-file-as-new-bugs) accepted for this release, or are any of them blockers?
3. Which build options is the build under test using?
4. Which baseline results (run A) already exist, so they need not be repeated?
