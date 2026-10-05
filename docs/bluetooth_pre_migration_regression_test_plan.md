# Bluetooth Pre-Migration Regression Test Plan

**Companion to:** [paired_device_migration_manual_test_plan.md](./paired_device_migration_manual_test_plan.md)
**Feature Flag:** `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` (compile-time)
**Scenario under test:** `performMigration` is **never** called (default state for any device that boots a migration-capable build but whose IUI/AS client has not yet adopted the new APIs).

---

## Purpose

The migration/rollback feature (`performMigration` / `clearMigration`) is **client-triggered**, not automatic. A device can run migration-capable firmware indefinitely without any client ever calling `performMigration`. This plan verifies that, in that default/untriggered state, the migration feature is completely **benign**:

- All pre-existing (legacy) Bluetooth plugin behavior continues to work exactly as it did before the migration feature was added.
- The only observable differences are the explicitly-guarded APIs (`setAutoConnect`, `getAutoConnect`, and the derived `autoconnect` field they feed), which fail closed / report disabled by design.
- No new persistence (`PersistentStore` `deviceInfo`/`migrationVersion`, or the AS filesystem file) is ever written.
- No new runtime behavior (e.g. power-mode-triggered disconnects) activates.

**Out of scope:** The mechanics of `performMigration`/`clearMigration` themselves (import correctness, rollback, reboot persistence, etc.) are covered by [paired_device_migration_manual_test_plan.md](./paired_device_migration_manual_test_plan.md). This plan assumes `performMigration` is **never** invoked at any point.

---

## Guarded APIs — Quick Reference

These are the **only** code paths in `BluetoothDeviceManager`/`Bluetooth` that check migration state (`_isMigrated`). Everything else is expected to behave identically regardless of migration state.

| Code path | Behavior when migration has **not** been performed |
|-----------|------------------------------------------------------|
| `setAutoConnect` | Rejected immediately: returns `{"result":{"success":false}}`. Logs `setAutoConnect rejected: migration has not been performed yet for deviceID=<ID>`. |
| `getAutoConnect` | Bypasses the cache entirely and returns `{"success":true,"autoconnect":false}` for **any** deviceID. Logs `migration not complete, returning disabled for deviceID=<ID>`. |
| `addDevice` (pairing) | In-memory cache is updated; the `PersistentStore` write is **skipped**. Logs `migration not complete, skipping persistence write for deviceID=<ID>`. The JSON-RPC `pair` call still reports success. |
| `removeDevice` (unpairing) | Same as above — cache updated, PersistentStore write skipped, `unpair` call still reports success. |
| `setLastConnectTimeUtc` (called internally on `connect`) | In-memory cache is updated; the `PersistentStore`/AS-file write is skipped. Logs `migration not complete, skipping persistence write for deviceID=<ID>`. |
| `setLastVolumeSetting` (called internally on `setDeviceVolumeMuteInfo`) | Same as above. |
| `writeStorageFromCache` → `writeFilesystemPersistenceFromCache` | The AS filesystem file write is only ever triggered from inside `writeStorageFromCache`, and only `if (_isMigrated)`. Pre-migration, this code path is simply never reached. |
| `onPowerModeChanged` | Returns immediately (`if (!isMigrated()) return;`) before evaluating any power-state transition. The entire power-mode-triggered disconnect feature (`disconnectExternallyConnectedDevices`) is inert pre-migration. |
| Plugin `init()` | Cache is **not** warmed from `PersistentStore` even if stale `deviceInfo` exists from a different firmware; init logs `migration not yet complete, ignoring any stale store data` and leaves the cache empty. |

Everything else — `startScan`/`stopScan`, `getDiscoveredDevices`, `getPairedDevices`/`getConnectedDevices` (device enumeration itself, which is always sourced live from BTRMGR, not the cache), `connect`/`disconnect`, `pair`/`unpair` (BTRMGR side-effects), `enable`/`disable`, `setDiscoverable`/`isDiscoverable`, `getName`/`setName`, `setAudioStream`, `sendAudioPlaybackCommand`, `respondToEvent`, `getDeviceInfo`, `getAudioInfo`, `getDeviceVolumeMuteInfo`/`setDeviceVolumeMuteInfo` (the actual BTRMGR volume/mute control), and all plugin events — has **no** migration-state check anywhere in the call path.

---

## Prerequisites

| # | Item |
|---|------|
| P1 | Device running a migration-capable firmware build (`BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` compiled in). |
| P2 | Confirm on boot the plugin logs `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is enabled`. |
| P3 | Confirm `performMigration` has **never** been called on this device: `Bluetooth/migrationVersion` is absent from PersistentStore. If present, run `clearMigration` once and reboot before starting this plan — this plan requires migration to remain untriggered for its full duration. |
| P4 | Access to device shell (SSH/serial) and device logs. |
| P5 | At least 2 Bluetooth devices available for pairing (at least one audio device; a keyboard/remote and a gamepad are useful for Section 9, if available). |
| P6 | AS persistent file path known: `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`. |

**Setup — confirm pre-migration state before starting:**
```bash
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc
# Expected: error response (key not found) — confirms performMigration has not run
```

**IMPORTANT:** Do **not** call `performMigration` at any point during this test plan. If it is accidentally called, the remaining test cases are invalid until `clearMigration` is called and the device is rebooted to re-establish the pre-migration baseline (reboot is required because `_isMigrated` is also cached in memory and only re-derived from PersistentStore at `init()`).

---

## Section 1: Baseline / Initial State Verification

### TC-PREREG-01: Fresh boot never calls performMigration automatically

**Steps:**
1. Reboot the device.
2. Search plugin logs for the init sequence.
3. Confirm `Bluetooth/migrationVersion` is still absent from PersistentStore after boot.

**Expected Results:**
- Logs show `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is enabled`.
- Logs show `Migration state at init: _isMigrated=false`.
- Logs show `migration not yet complete, ignoring any stale store data`.
- `migrationVersion` remains absent from PersistentStore — migration is never auto-triggered.
- Plugin activates successfully with no errors.

---

### TC-PREREG-02: PersistentStore deviceInfo is not auto-populated on boot

**Steps:**
1. After boot (TC-PREREG-01), read `Bluetooth/deviceInfo` from PersistentStore:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- `deviceInfo` is absent, or contains only stale data from a prior (non-migration) firmware — it is not written to or refreshed by this boot.

---

## Section 2: Discovery & Scanning Regression

### TC-PREREG-03: startScan / stopScan / getDiscoveredDevices behave normally

**Steps:**
1. Call `startScan`.
2. Verify `onDeviceFound`/`onDeviceLost`/`onDiscoveredDevice` events fire as devices are found.
3. Call `getDiscoveredDevices` and confirm discovered devices are listed.
4. Call `stopScan`.

**Expected Results:**
- All calls behave identically to a non-migration-capable build. No migration-related errors or rejections.

---

## Section 3: Pairing & Unpairing Regression

### TC-PREREG-04: Pairing succeeds and is immediately visible via getPairedDevices

**Steps:**
1. Call `pair` for a discovered device.
2. Call `getPairedDevices`.
3. Check `Bluetooth/deviceInfo` in PersistentStore.

**Expected Results:**
- `pair` returns success.
- The device appears in `getPairedDevices` — this list is always sourced live from BTRMGR, so it reflects the new pairing regardless of migration state.
- `Bluetooth/deviceInfo` in PersistentStore is **not** updated (the cache was updated in-memory only).

**Expected Log Entries:**
- `migration not complete, skipping persistence write for deviceID=<ID>`

---

### TC-PREREG-05: Unpairing succeeds and is immediately reflected

**Steps:**
1. Call `unpair` for the device paired in TC-PREREG-04.
2. Call `getPairedDevices`.
3. Check `Bluetooth/deviceInfo` in PersistentStore.

**Expected Results:**
- `unpair` returns success.
- The device no longer appears in `getPairedDevices`.
- `Bluetooth/deviceInfo` in PersistentStore remains unchanged (no write attempted).

**Expected Log Entries:**
- `migration not complete, skipping persistence write for deviceID=<ID>`

---

### TC-PREREG-06: AS filesystem file is never touched by pairing activity

**Precondition:** Record a checksum of the AS file before this test.
```bash
md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
```

**Steps:**
1. Pair a device.
2. Unpair the device.
3. Re-check the AS file checksum.

**Expected Results:**
- AS file checksum is unchanged across both operations — the filesystem sync path (`writeFilesystemPersistenceFromCache`) is only reachable through `writeStorageFromCache()`, which is only invoked for migrated pairing/unpairing writes; pre-migration, `addDevice`/`removeDevice` return before calling it.

---

## Section 4: Connection Management Regression

### TC-PREREG-07: connect / disconnect behave normally for a paired device

**Steps:**
1. Pair a device (if not already paired).
2. Call `connect` for the device.
3. Call `getConnectedDevices` and confirm the device is listed as connected.
4. Call `disconnect`.
5. Call `getConnectedDevices` again and confirm the device is no longer listed.

**Expected Results:**
- `connect`/`disconnect` succeed and behave identically to a non-migration-capable build.
- `getConnectedDevices` output is sourced live from BTRMGR (`BTRMGR_GetConnectedDevices`) and is accurate regardless of migration state.

**Expected Log Entries (on connect):**
- `migration not complete, skipping persistence write for deviceID=<ID>` (from the internal `setLastConnectTimeUtc` call triggered by a successful connect)

---

## Section 5: AutoConnect Guard Behavior (Expected Divergence — Not a Defect)

> These are the **only** functional differences from a non-migration-capable build. QA should confirm they occur exactly as described — and should **not** file these as regressions.

### TC-PREREG-08: setAutoConnect is always rejected

**Steps:**
1. Obtain a paired `deviceID` via `getPairedDevices`.
2. Call `setAutoConnect` with `enable:true`.
3. Call `setAutoConnect` with `enable:false`.

**Expected Results:**
- Both calls return `{"result":{"success":false}}`.
- Neither call modifies PersistentStore or the AS file.

**Expected Log Entries:**
- `setAutoConnect rejected: migration has not been performed yet for deviceID=<ID>`

---

### TC-PREREG-09: getAutoConnect always reports disabled

**Steps:**
1. Call `getAutoConnect` for any paired `deviceID` (including one that was never explicitly configured for autoConnect).
2. Call `getAutoConnect` for a `deviceID` that does not exist at all.

**Expected Results:**
- Both calls return `{"success":true,"autoconnect":false}` — `getAutoConnect` never errors pre-migration, even for a non-existent device, because it bypasses the cache lookup entirely.

**Expected Log Entries:**
- `migration not complete, returning disabled for deviceID=<ID>`

---

### TC-PREREG-10: getPairedDevices / getConnectedDevices always report autoconnect=false

**Steps:**
1. Pair and connect a device.
2. Call `getPairedDevices` and `getConnectedDevices`.
3. Inspect the `autoconnect` field for the device in both responses.

**Expected Results:**
- The `autoconnect` field is present and `false` for every device in both responses — both enumeration methods derive this field by calling `getAutoConnect` internally, which is guarded as described in TC-PREREG-09. This is expected and does **not** indicate an actual auto-connect preference of `false`; it simply means auto-connect state cannot be honored until migration is performed.

---

## Section 6: Device Info, Audio & Playback Regression

### TC-PREREG-11: getDeviceInfo / getAudioInfo report normally

**Steps:**
1. Call `getDeviceInfo` for a connected device.
2. Call `getAudioInfo` for a connected audio device.

**Expected Results:**
- Both return accurate, unaffected data — neither path touches migration state.

---

### TC-PREREG-12: setAudioStream and sendAudioPlaybackCommand behave normally

**Steps:**
1. Call `setAudioStream` with `PRIMARY` and then `AUXILIARY`.
2. Call `sendAudioPlaybackCommand` with each of `PLAY`, `PAUSE`, `STOP`, `RESUME`, `SKIP_NEXT`, `SKIP_PREV`, `VOLUME_UP`, `VOLUME_DOWN`, `AUDIO_MUTE`, `AUDIO_UNMUTE`.

**Expected Results:**
- All commands succeed and produce the same BTRMGR-level behavior as a non-migration-capable build.

---

### TC-PREREG-13: respondToEvent (pairing/connection request responses) behaves normally

**Steps:**
1. Trigger an incoming pairing request from a remote device (or simulate via `onPairingRequest`).
2. Call `respondToEvent` to accept or reject.

**Expected Results:**
- The response is honored normally; pairing proceeds (or is rejected) exactly as on a non-migration-capable build. (Any resulting `pair` call still follows the guarded-persistence behavior from Section 3.)

---

## Section 7: Volume & Mute Regression

### TC-PREREG-14: getDeviceVolumeMuteInfo / setDeviceVolumeMuteInfo control real volume/mute state

**Steps:**
1. Call `setDeviceVolumeMuteInfo` to set a new volume level for a connected device.
2. Call `getDeviceVolumeMuteInfo` and confirm the device reports the new volume via BTRMGR.
3. Check `Bluetooth/deviceInfo` in PersistentStore.

**Expected Results:**
- The device's actual volume changes via BTRMGR (audible/observable effect preserved).
- `getDeviceVolumeMuteInfo` reflects the new level.
- `Bluetooth/deviceInfo` in PersistentStore is **not** updated — `setLastVolumeSetting` skips the persistence write pre-migration.

**Expected Log Entries:**
- `migration not complete, skipping persistence write for deviceID=<ID>`

---

## Section 8: Adapter Control Regression

### TC-PREREG-15: enable / disable / setDiscoverable / isDiscoverable / getName / setName behave normally

**Steps:**
1. Call `disable`, then `enable` the Bluetooth adapter.
2. Call `setDiscoverable` with `true`, then `isDiscoverable` to confirm.
3. Call `getName`, then `setName` with a new friendly name, then `getName` again to confirm.

**Expected Results:**
- All calls succeed and produce identical results to a non-migration-capable build — none of these paths reference migration state.

---

## Section 9: Gamepad / HID Classification Regression

### TC-PREREG-16: HID and gamepad devices pair/connect normally pre-migration

**Steps:**
1. Pair a non-gamepad HID device (keyboard/remote).
2. Pair a Bluetooth gamepad (if available).
3. Call `getPairedDevices` and `getConnectedDevices` for both.

**Expected Results:**
- Both device types pair, connect, and enumerate normally.
- Neither device's entry is written to PersistentStore or the AS file (per Section 3/6), so the gamepad-vs-HID filesystem-sync distinction documented in the companion migration test plan (TC-RB-04) is **not observable** pre-migration — there is nothing to observe, since no filesystem sync ever occurs until migration is active.

---

## Section 10: Power Management Guard

### TC-PREREG-17: onPowerModeChanged is a full no-op pre-migration

**Precondition:** At least one non-HID device (e.g. an audio device) is connected.

**Steps:**
1. Trigger a power state transition (e.g. ON → STANDBY, or STANDBY → ON) via the platform's power manager.
2. Observe plugin logs during the transition.
3. Confirm the previously-connected device remains connected after the transition (i.e. is not force-disconnected).

**Expected Results:**
- The plugin's `onPowerModeChanged` handler returns immediately, before logging the power-state transition message or evaluating any disconnect logic — because `isMigrated()` is `false`.
- `disconnectExternallyConnectedDevices()` is never invoked.
- Connected devices are **not** force-disconnected by the plugin as a side effect of the power transition (this disconnect-on-power-transition behavior is new functionality introduced for migration and only activates once migration is active — it is not a feature of pre-migration-capable builds either, so its absence here is expected, not a regression).

---

## Section 11: Persistence Non-Interference (Cumulative Check)

### TC-PREREG-18: No PersistentStore or AS-file writes occur across an entire regression pass

**Steps:**
1. Before starting Sections 2–10, record:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
     http://127.0.0.1:9998/jsonrpc
   md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
2. Execute all test cases in Sections 2–10 (pairing, unpairing, connecting, disconnecting, volume changes, etc.).
3. Re-check both values.

**Expected Results:**
- `Bluetooth/deviceInfo` in PersistentStore is identical to its value before the pass (unchanged, or still absent).
- `Bluetooth/migrationVersion` remains absent from PersistentStore.
- The AS file checksum is identical to its value before the pass.
- No log line `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N` appears anywhere in the logs for this pass.

---

## Section 12: Plugin Lifecycle / Reboot Stability

### TC-PREREG-19: Repeated activate/deactivate cycles remain stable pre-migration

**Steps:**
1. Deactivate the Bluetooth plugin.
2. Reactivate the Bluetooth plugin.
3. Repeat 2–3 times.
4. Confirm plugin state and functionality after each cycle (e.g. `getApiVersionNumber`, `getPairedDevices`).

**Expected Results:**
- Plugin activates/deactivates cleanly every cycle, with no crashes, with `migrationVersion` remaining absent throughout.

---

### TC-PREREG-20: Reboot stability with migration never triggered

**Steps:**
1. Reboot the device.
2. Repeat TC-PREREG-01 and TC-PREREG-02.
3. Spot-check one case each from Sections 2–9 (e.g. scan, pair, connect, volume, setAutoConnect rejection).

**Expected Results:**
- Behavior after reboot is identical to the first pass — migration state is correctly re-derived as `false` at every boot until `performMigration` is explicitly called.

---

## Exit Criteria / Sign-off Checklist

- [ ] All Section 1–4, 6–9, 11–12 test cases pass with behavior **identical** to a non-migration-capable build.
- [ ] All Section 5 and 10 test cases pass with the **documented, guard-driven divergence** (and only that divergence).
- [ ] No PersistentStore `deviceInfo`/`migrationVersion` writes or AS-file writes occur anywhere during the pass.
- [ ] No crashes, unhandled exceptions, or unexpected error logs attributable to the migration feature being compiled in.
- [ ] `performMigration` was confirmed to have never been called at the start and end of the pass.
