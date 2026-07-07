# Bluetooth Migration & Rollback — Manual Test Plan

**Ticket:** RDKEMW-20057 / CPESP-9979
**Feature Flag:** `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` (compile-time)  
**AS Persistent File:** `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`  
**PersistentStore Location:** namespace `Bluetooth`, key `deviceInfo`  
**PersistentStore Checksum Key:** namespace `Bluetooth`, key `migrationVersion`

---

## Overview

Migration is **client-triggered**, not automatic. The plugin does **not** auto-migrate at init. The IUI/AS client is responsible for calling the appropriate API on first boot. Since no client (IUI/AS) currently calls these APIs, the tester simulates client behaviour directly via curl.

### API Summary

| API | Method | Description |
|-----|--------|-------------|
| `performMigration` | `org.rdk.Bluetooth.performMigration` | **First call:** imports AS file → RDK PersistentStore, writes `migrationVersion=1`. **Subsequent calls:** no-op (migrationVersion already present). |
| `clearMigration` | `org.rdk.Bluetooth.clearMigration` | Deletes `deviceInfo` and `migrationVersion` from RDK PersistentStore, clears the plugin's paired device cache, and resets migration state. AS file is **not** touched. |

### Migration State

The plugin tracks whether migration has been performed. Migration state becomes active after a successful `performMigration` call and is cleared by `clearMigration`. Key observable effects:

- `setAutoConnect` is **rejected** (returns a JSON-RPC error response) when migration has not been performed.
- `setAutoConnect` changes will **not** be written to PersistentStore when migration has not been performed.
- Pairing and unpairing (`addDevice`/`removeDevice`) always write to PersistentStore regardless of migration state.
- The AS file (filesystem persistence) will **not** be updated by any operation — pairing, unpairing, or `setAutoConnect` — when migration has not been performed.
- `getAutoConnect` has no migration guard. However, since `clearMigration` also wipes the in-memory device cache, `getAutoConnect` calls will fail with a device-not-found error (not a migration rejection) until the cache is repopulated (e.g. after a reboot or `performMigration`).

### Curl Reference Commands

```bash
# Invoke performMigration (simulating IUI "LD Enabled" first-boot behaviour)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# Invoke clearMigration (simulating IUI "LD Disabled" / rollback behaviour)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# Read deviceInfo from PersistentStore
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
  http://127.0.0.1:9998/jsonrpc

# Read migrationVersion from PersistentStore
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc

# Delete a key from PersistentStore (manual cleanup)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.deleteKey","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
  http://127.0.0.1:9998/jsonrpc

# Set autoConnect for a device (requires prior performMigration)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
  http://127.0.0.1:9998/jsonrpc

# Get autoConnect for a device
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.getAutoConnect","params":{"deviceID":"<ID>"}}' \
  http://127.0.0.1:9998/jsonrpc

# Get paired devices
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.getPairedDevices"}' \
  http://127.0.0.1:9998/jsonrpc
```

---

## Prerequisites

| # | Item |
|---|------|
| P1 | Verify that upon start-up, the Bluetooth plugin logs contain `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is enabled`. This confirms the device is running a migration-capable firmware build, which is required for all test cases in this plan. |
| P2 | Verify that upon start-up, the Bluetooth plugin logs confirm the AS persistence file path is `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json` |
| P3 | Access to device shell (SSH/serial) |
| P4 | At least 2 Bluetooth audio devices available for pairing |
| P5 | "Old" firmware image (pre-migration, AS-managed persistence) available for rollback tests |
| P6 | "New" firmware image (migration-enabled build) available |
| P7 | Ability to reboot device and flash firmware |

---

## IMPORTANT NOTE

For any test cases requiring a change in the bluetooth device's auto-connect status, the guide UI should **NOT** be used. The UI isn't currently using the new Bluetooth Thunder plug-in APIs to set the auto-connect status, and as such would not be reflected in PersistentStore. Use the `setAutoConnect` curl command from the Curl Reference Commands section above.

`setAutoConnect` will be **rejected** (returns a JSON-RPC error: `{"error":{"code":1,"message":"ERROR_GENERAL"}}`) until a successful `performMigration` has occurred since the last `clearMigration`, or equivalently, until the `migrationVersion` key is present in PersistentStore. Because the plugin re-derives migration state from that key at init, migration remains enabled across reboots without needing `performMigration` to be called again.

---

## Section 1: performMigration — Initial Migration

### TC-MIG-01: First-time migration from AS filesystem to PersistentStore

**Precondition:** Device has paired devices in the AS file `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`. Both `Bluetooth/deviceInfo` and `Bluetooth/migrationVersion` are absent from PersistentStore (clean state, simulating first boot on new firmware).

**Setup:**
```bash
# Ensure clean PS state
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.deleteKey","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
  http://127.0.0.1:9998/jsonrpc

curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.deleteKey","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc

# Confirm pre-existing AS file
cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
```
Note the device addresses, `autoConnectStatus`, and `lastConnectionTimeUTC` values listed in the file.

**Steps:**
1. Reboot device and wait for Bluetooth plugin activation.
2. Verify `migrationVersion` is absent from PS (the authoritative indicator that migration has not been performed). Note: `deviceInfo` is NOT a reliable pre-migration indicator — plugin `init` always writes paired device data from BTRMGR to PS, so `deviceInfo` will already be present after the reboot in step 1:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
     http://127.0.0.1:9998/jsonrpc
   # Expected: JSON-RPC error response (key not found)
   ```
2a. Verify that `migrationVersion` is absent from PS (the authoritative indicator that migration has not been performed). Note that `deviceInfo` **will** be present at this point — plugin `init` always writes paired device data from BTRMGR to PS regardless of migration state:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
     http://127.0.0.1:9998/jsonrpc
   # Expected: JSON-RPC error response (key not found)
   ```
3. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Verify `deviceInfo` is now populated in PersistentStore with data imported from the AS file (as opposed to the BTRMGR-only data that was there before):
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
5. Verify `migrationVersion` is now stored in PersistentStore:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
6. Verify the AS file is semantically equivalent to its pre-migration state. Note: `performMigration` calls `writeFilesystemPersistenceFromCache()` after a successful PersistentStore write, so the file may be rewritten or reformatted (canonicalized) — byte equality and mtime checks are **not** appropriate here. Instead, compare content against the values noted in Setup:
   ```bash
   cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
   Confirm that every device address, `autoConnectStatus`, and `lastConnectionTimeUTC` recorded before migration is still present and correct.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `Bluetooth/deviceInfo` is populated with device entries matching those from the AS file.
- `Bluetooth/migrationVersion` is set to `"1"`.
- Device `autoConnectStatus` and `lastConnectionTimeUTC` values from the AS file are preserved in PS.
 - The AS file may be re-written by rollback-sync, but it should remain semantically equivalent (same pairedDevices data unless expected backfill/normalization occurs).

**Expected Log Entries:**
- `performMigration: initial migration succeeded`
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N`

---

### TC-MIG-02: performMigration is a no-op when migrationVersion is already present

**Precondition:** TC-MIG-01 has been completed successfully. Both `deviceInfo` and `migrationVersion` are present in PS.

**Steps:**
1. Record the current `migrationVersion` value.
2. Record the current `deviceInfo` value.
3. Call `performMigration` again **without modifying the AS file**:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Read `deviceInfo` and `migrationVersion` again and compare to values from step 1–2.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `deviceInfo` is **unchanged**.
- `migrationVersion` is **unchanged**.
- No new import occurs.

**Expected Log Entries:**
- `performMigration: migration already completed (migrationVersion present), no sync needed`

---

### TC-MIG-03: performMigration is always a no-op after first migration (migrationVersion present)

> **Note:** The re-sync-on-file-change behaviour (checksum-based) has been removed. After the first successful `performMigration`, subsequent calls are always no-ops regardless of AS file content.

**Precondition:** TC-MIG-01 completed. `deviceInfo` and `migrationVersion` are present in PS.

**Steps:**
1. Record the current `migrationVersion` value and number of devices in `deviceInfo`.
2. Manually add a valid device entry to the AS file:
   ```bash
   # Edit the file — add a device entry to pairedDevices array
   # Use a real MAC address matching a device paired to the platform
   cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   # Modify and save
   ```
3. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Read `deviceInfo` and `migrationVersion` from PS.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `migrationVersion` is **unchanged** (`"1"`) — no re-import occurs.
- `deviceInfo` is **unchanged** — the modified AS file is ignored.

**Expected Log Entries:**
- `performMigration: migration already completed (migrationVersion present), no sync needed`

---

### TC-MIG-04: performMigration with empty AS file

**Precondition:** Both `deviceInfo` and `migrationVersion` are absent from PS.

**Steps:**
1. Write a valid but empty AS file:
   ```bash
   echo '{"pairedDevices":[]}' > /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
2. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
3. Read `deviceInfo` from PS.
4. Read `migrationVersion` from PS.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `deviceInfo` is present but contains an empty array (`[]`).
- `migrationVersion` is present and set to `"1"`.
- Plugin continues operating normally.

**Expected Log Entries:**
- `performMigration: initial migration succeeded`
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=0`

---

### TC-MIG-05: performMigration when AS file does not exist

**Precondition:** Both `deviceInfo` and `migrationVersion` are absent from PS. AS file is deleted.

**Steps:**
1. Remove the AS file:
   ```bash
   rm -f /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
2. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
3. Read `deviceInfo` from PS.
4. Read `migrationVersion` from PS.

**Expected Results:**
- `performMigration` returns `{"success": true}` (absent file treated as empty, not an error).
- `deviceInfo` is present and contains an empty or minimal array.
- `migrationVersion` is present and set to `"1"`.

**Expected Log Entries:**
- `filesystem persistence file does not exist: /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json` (logged by the persistence adapter when the file is not found; absent file is treated as empty — no error is returned)
- `performMigration: initial migration succeeded`

---

## Section 2: clearMigration — Rollback

### TC-CLR-01: clearMigration wipes PersistentStore and resets migration state

**Precondition:** `performMigration` has been successfully called. `deviceInfo` and `migrationVersion` are both present in PS.

**Steps:**
1. Confirm both PS keys exist (see Curl Reference Commands).
2. Call `clearMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
3. Verify `deviceInfo` is gone from PS:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
     http://127.0.0.1:9998/jsonrpc
   # Expected: key absent / error
   ```
4. Verify `migrationVersion` is gone from PS:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
     http://127.0.0.1:9998/jsonrpc
   # Expected: key absent / error
   ```
5. Verify the AS file is **unchanged**:
   ```bash
   cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```

**Expected Results:**
- `clearMigration` returns `{"success": true}`.
- `Bluetooth/deviceInfo` is absent from PS.
- `Bluetooth/migrationVersion` is absent from PS.
- AS file is intact and unmodified.
- In-memory device cache is cleared (`getPairedDevices` will return no entries).

**Expected Log Entries:**
- `clearMigration: PersistentStore cleared and migration state reset`

---

### TC-CLR-02: clearMigration on already-empty PersistentStore (idempotent)

**Precondition:** Neither `deviceInfo` nor `migrationVersion` exist in PS (either a clean device, or after a prior `clearMigration`).

**Steps:**
1. Confirm both PS keys are absent.
2. Call `clearMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- `clearMigration` returns `{"success": true}` — deleting absent keys is not an error.

**Expected Log Entries:**
- `clearMigration: PersistentStore cleared and migration state reset`

---

### TC-CLR-03: clearMigration then performMigration re-migrates successfully

**Precondition:** A prior migration has been completed. AS file has paired device entries.

**Steps:**
1. Call `clearMigration` to reset state.
2. Verify both PS keys are absent.
3. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Verify `deviceInfo` and `migrationVersion` are present in PS again.
5. Verify device data matches the AS file.

**Expected Results:**
- After `clearMigration`, both keys are absent.
- After `performMigration`, both keys are restored with data from the AS file.
- `performMigration` returns `{"success": true}`.

**Expected Log Entries (step 3):**
- `performMigration: initial migration succeeded` (treated as first time since checksum key was absent)

---

## Section 3: Pre-Migration Guard on setAutoConnect

### TC-GUARD-01: setAutoConnect rejected before performMigration

**Precondition:** Migration has not been performed — either the device has never had `performMigration` called, or `clearMigration` has been called since the last `performMigration`. Verify this by confirming `migrationVersion` is absent from PersistentStore.

**Setup (ensure pre-migration state):**
```bash
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc
```

**Steps:**
1. Obtain a paired device ID:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.getPairedDevices"}' \
     http://127.0.0.1:9998/jsonrpc
   ```
   Note one `deviceID` from the result.
2. Attempt to call `setAutoConnect` without first calling `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- Response returns a JSON-RPC error: `{"error":{"code":1,"message":"ERROR_GENERAL"}}`.
- No changes are made to PS.

**Expected Log Entries:**
- `setAutoConnect rejected: migration has not been performed yet for deviceID=<ID>`

---

### TC-GUARD-02: setAutoConnect succeeds after performMigration

**Precondition:** Continuing from TC-GUARD-01 (migration state is reset).

**Steps:**
1. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Call `setAutoConnect` with the same device ID from TC-GUARD-01:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
3. Verify the change is reflected in PS `deviceInfo` (`autoconnect: 1`).
4. Verify the AS file also reflects `"autoConnectStatus": true` for the corresponding device address.

**Expected Results:**
- `setAutoConnect` returns `{"success": true}`.
- PS `deviceInfo` shows `"autoconnect": 1` for that device.
- AS file is written with the updated `autoConnectStatus`.

---

### TC-GUARD-03: setAutoConnect rejected again after clearMigration

**Precondition:** `performMigration` has been completed (TC-GUARD-02 passed).

**Steps:**
1. Call `clearMigration` to reset migration state:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Attempt `setAutoConnect` on the same device:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.setAutoConnect","params":{"deviceID":"<ID>","enable":false}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- `setAutoConnect` returns a JSON-RPC error: `{"error":{"code":1,"message":"ERROR_GENERAL"}}`.
- Rejection is immediate without attempting a PS write.

**Expected Log Entries:**
- `setAutoConnect rejected: migration has not been performed yet for deviceID=<ID>`

---

### TC-GUARD-04: getAutoConnect is unaffected by migration state

**Precondition:** `clearMigration` has been called (migration state is false).

**Steps:**
1. Call `getAutoConnect` for a known paired device:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.getAutoConnect","params":{"deviceID":"<ID>"}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- `getAutoConnect` is NOT rejected with a migration guard error (unlike `setAutoConnect`, there is no `_isMigrated` check in `getAutoConnect`).
- However, because `clearMigration` wiped the in-memory device cache, `getAutoConnect` will fail with a device-not-found error (`{"error":{"code":1,"message":"ERROR_GENERAL"}}`) rather than returning a value. This failure is caused by the empty cache — **not** by a migration guard. This distinguishes it from `setAutoConnect`, which fails specifically because of the migration guard.
- To observe `getAutoConnect` succeed with migration state false, reboot the device first (init repopulates the cache from BTRMGR). After reboot, `getAutoConnect` will succeed and return `{"autoconnect": false}` for any device whose auto-connect status was not set.

---

## Section 4: Rollback Synchronization (Ongoing Filesystem Sync)

### TC-RB-01: New device pairing updates both PersistentStore AND filesystem

**Precondition:** `performMigration` has been completed. Both stores are in sync.

**Steps:**
1. Note current content of both stores.
2. Pair a new Bluetooth device.
3. Check PersistentStore:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Check filesystem file:
   ```bash
   cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```

**Expected Results:**
- New device appears in PersistentStore with `deviceID`, `deviceType`, and `autoconnect` fields.
- New device appears in filesystem file with `deviceAddr`, `friendlyName`, `deviceType`, `autoConnectStatus`, `lastVolumeSetting`, `lastConnectionTimeUTC` fields.
- Schema of filesystem file remains valid per `docs/paired_bluetooth_devices.schema.json`.

**Expected Log Entries:**
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N`

---

### TC-RB-02: autoConnect change updates both stores

**Precondition:** `performMigration` has been completed.

**Steps:**
1. Set `autoConnect` for a paired device:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Verify PersistentStore reflects `"autoconnect": 1`.
3. Verify filesystem file reflects `"autoConnectStatus": true` for that device address.

**Expected Results:**
- Both stores consistently reflect the `autoConnect` change.

**Expected Log Entries:**
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N`

---

### TC-RB-03: Device removal updates both stores

**Precondition:** `performMigration` has been completed.

**Steps:**
1. Unpair a device.
2. Check PersistentStore — device entry should be removed.
3. Check filesystem file — device entry should be removed.

**Expected Results:**
- Device is absent from both stores.

**Expected Log Entries:**
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N`

---

### TC-RB-04: HID devices excluded from filesystem sync

**Steps:**
1. Call `performMigration`.
2. Pair a Human Interface Device (e.g., BT keyboard/remote identified as "HUMAN INTERFACE DEVICE").
3. Verify it appears in PersistentStore `deviceInfo`.
4. Verify it does **NOT** appear in the filesystem file.

**Expected Results:**
- HID devices are in PersistentStore but excluded from the filesystem file (matching legacy AS behaviour).

---

### TC-RB-05: AS file is NOT modified when performMigration has never been called

**Precondition:** Fresh state — `performMigration` has never been called in this session. Both `deviceInfo` and `migrationVersion` are absent from PS (use `clearMigration` to reset if needed). The AS file exists and has known content.

**Setup:**
```bash
# Reset to pre-migration state
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# Record a checksum of the AS file before the test
md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
# OR note the last-modified timestamp
ls -la /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
```

**Steps:**
1. Pair a new Bluetooth audio device.
2. Verify PS `deviceInfo` IS updated (pairing always writes to PersistentStore regardless of migration state — this is expected):
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' \
     http://127.0.0.1:9998/jsonrpc
   # Expected: success — deviceInfo is present and includes the newly paired device
   ```
   Verify that `migrationVersion` is still absent (migration has not been performed).
3. Verify the AS file is **unchanged** (compare checksum or mtime to the baseline recorded in Setup):
   ```bash
   md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ls -la /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
4. Unpair the device.
5. Verify the AS file is still unchanged.

**Expected Results:**
- PersistentStore `deviceInfo` is updated on pairing and unpairing (pairing/unpairing are not guarded by migration state).
- The AS file is **not** modified at any point — only the AS file is protected by migration state.
- AS file checksum and mtime are identical to the baseline recorded before step 1.

**Expected Log Entries** (no migration-specific skip log exists for `writeStorageFromCache`; search device logs for the absence of the filesystem sync success message):
- Confirm that `Filesystem persistence sync succeeded` does **not** appear in device logs during this test (the AS file write is skipped silently when `_isMigrated` is false).

---

### TC-RB-06: AS file is NOT modified after clearMigration (until performMigration is called again)

**Precondition:** `performMigration` has previously been completed. Devices are paired. Both PS and the AS file are in sync. At least one device is paired with a known `autoConnectStatus`.

**Setup:**
```bash
# Record a checksum of the AS file before the test
md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
```

**Steps:**
1. Call `clearMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Verify PS `deviceInfo` is gone.
3. Verify the AS file is **unchanged** after `clearMigration` itself:
   ```bash
   md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
4. Pair a new Bluetooth device. Verify the AS file is still unchanged.
5. Unpair the device. Verify the AS file is still unchanged.
6. Now call `performMigration` to restore migration state:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
7. Pair another new Bluetooth device. Verify the AS file **is** now updated (migration is active again):
   ```bash
   md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```

**Expected Results:**
- `clearMigration` does not modify the AS file (steps 1–3: checksum unchanged).
- Pairing and unpairing operations in steps 4–5 do not modify the AS file, because migration has not been re-established.
- After `performMigration` in step 6, the next pairing operation (step 7) **does** update the AS file (new device appears).

**Expected Log Entries for steps 4 and 5**: There is no dedicated skip-log for the AS file write when migration has not been performed. Confirm instead that `Filesystem persistence sync succeeded` does **not** appear in logs during steps 4–5.

**Expected Log Entries for step 7** (search device logs for this exact string):
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N`

---

## Section 5: Firmware Rollback Scenario

### TC-ROLL-01: Rollback — downgrade firmware and verify AS reads correct data

**Precondition:** Device running new firmware. `performMigration` has been called. Several devices paired.

**Steps:**
1. Record current paired devices and their settings from both stores.
2. Pair 1–2 NEW devices on the new firmware (ensures they are written to both PS and the AS file for rollback continuity).
3. Verify both stores are in sync.
4. Flash/downgrade to the OLD firmware (pre-migration, AS-managed persistence).
5. Boot the device.
6. Verify AS Peripheral reads `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`.
7. Verify all devices — including those newly paired on new firmware — are recognised and connect correctly.

**Expected Results:**
- Old firmware boots and finds all paired devices in the filesystem file (which was kept in sync by new firmware during step 2).
- Devices auto-connect according to their saved `autoConnectStatus`.
- No device data is lost during the downgrade.

---

### TC-ROLL-02: Rollback with clearMigration (LD Disabled scenario)

**Precondition:** Device running new firmware. `performMigration` was previously called. Several devices paired.

This test simulates the "IUI LD Disabled" scenario from the design, where IUI calls `clearMigration` first (acting as a rollback trigger), then falls back to AS Peripheral.

**Steps:**
1. Record current AS file and PS contents.
2. Call `clearMigration` (simulating IUI LD Disabled first-boot behaviour):
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
3. Verify PS `deviceInfo` and `migrationVersion` are gone.
4. Verify the AS file is **unchanged** (clearing only affects PS, not the AS file).
5. Attempt `setAutoConnect` for any device — verify it returns a JSON-RPC error: `{"error":{"code":1,"message":"ERROR_GENERAL"}}`.
6. Downgrade to old firmware (or simulate old-firmware behaviour by verifying only the AS file is used).
7. Verify all devices in the AS file are recognised.

**Expected Results:**
- `clearMigration` succeeds.
- PS is empty; AS file is intact.
- Old firmware / AS Peripheral reads the filesystem file correctly.
- All devices are available with their saved settings.

---

### TC-ROLL-03: Round-trip — upgrade → pair → downgrade → upgrade again

**Steps:**
1. Start on OLD firmware with 2 paired devices.
2. Upgrade to NEW firmware.
3. Call `performMigration` → migration should import both devices (TC-MIG-01).
4. Pair 1 additional device on NEW firmware.
5. Verify all 3 devices are in both PS and the AS file.
6. Downgrade to OLD firmware → verify all 3 devices work via AS file.
7. Pair 1 more device on OLD firmware (written to AS file only).
8. Upgrade back to NEW firmware.
9. Reboot and call `performMigration` again.
10. Verify `performMigration` **re-syncs** (the AS file changed due to step 7 — `migrationVersion` key was absent after the second upgrade, so first-time migration runs again).
11. Verify the device paired in step 7 is now in PS.

**Expected Results:**
- No data loss at any transition point.
- On second upgrade (step 9), `clearMigration` (or absence of `migrationVersion`) allows `performMigration` to run a fresh first-time import from the updated AS file.
- All 4 devices are present in PS after second upgrade.

**Expected Log Entries (step 9):**
- `performMigration: initial migration succeeded`

---

## Section 6: Reboot Persistence Validation

### TC-REBOOT-01: PersistentStore and filesystem survive reboot after migration

**Precondition:** `performMigration` has been called. Devices are paired with `autoConnect` settings configured.

**Steps:**
1. Verify both stores have consistent data before reboot.
2. Reboot device.
3. After boot, verify `deviceInfo` and `migrationVersion` are still present in PS.
4. Verify AS file still contains expected data.
5. Verify devices auto-connect per their saved settings.
6. Verify that `performMigration` does NOT need to be called again (state persists across reboot).

**Expected Results:**
- All data survives reboot.
- `autoConnect` and `lastConnectionTimeUTC` are preserved.
- The plugin recognises that migration was previously completed (because `migrationVersion` is present in PersistentStore) and does not require `performMigration` to be called again.
- `setAutoConnect` works immediately after reboot without needing to re-call `performMigration`.

**Expected Log Entries on boot** (search device logs for these exact strings):
- `Migration state at init: _isMigrated=true`
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N` (the in-memory hash resets on process restart, so the AS file is always rewritten on the first `writeStorageFromCache` call after boot, even if the content is unchanged)

---

### TC-REBOOT-02: setAutoConnect rejected after reboot when migration was never performed

**Precondition:** Both `deviceInfo` and `migrationVersion` are absent from PersistentStore. `performMigration` has never been successfully called on this device.

**Steps:**
1. Delete both PS keys (or call `clearMigration`) and reboot.
2. After boot, attempt `setAutoConnect` for any paired device **before** calling `performMigration`.

**Expected Results:**
- `setAutoConnect` returns a JSON-RPC error: `{"error":{"code":1,"message":"ERROR_GENERAL"}}`.
- The plugin correctly identifies on boot that no prior migration has been performed (no checksum key in PersistentStore) and enforces the guard.

**Expected Log Entries on boot** (search device logs for this exact string):
- `Migration state at init: _isMigrated=false`

---

## Section 7: Edge Cases

### TC-EDGE-01: Device in AS file but no longer paired in BTRMGR

**Precondition:** AS file has a device entry with address `AA:BB:CC:DD:EE:FF`. That device is NOT in the platform's paired device list.

**Steps:**
1. Ensure both PS keys are absent.
2. Call `performMigration`.
3. Read `deviceInfo` from PS.

**Expected Results:**
- Device with address `AA:BB:CC:DD:EE:FF` is skipped during import because it is no longer in the platform's paired device list.
- Other valid devices import normally.
- `performMigration` returns `{"success": true}`.

**Expected Log Entries:**
- `performMigration: initial migration succeeded`
- `[WARN] No paired device handle found for addr=AA:BB:CC:DD:EE:FF during filesystem persistence import, skipping`

---

### TC-EDGE-02: performMigration called multiple times concurrently (stress)

**Steps:**
1. Ensure `performMigration` has not been called (no `migrationVersion` in PS).
2. Fire multiple concurrent `performMigration` calls in quick succession:
   ```bash
   for i in {1..5}; do
     curl --header "Content-Type: application/json" --request POST \
       --data '{"jsonrpc":"2.0","id":'"$i"',"method":"org.rdk.Bluetooth.performMigration","params":{}}' \
       http://127.0.0.1:9998/jsonrpc &
   done
   wait
   ```
3. Verify `deviceInfo` and `migrationVersion` are present and contain valid data.
4. Verify no duplicate device entries in `deviceInfo`.

**Expected Results:**
- All calls return `{"success": true}`.
- Only one import occurs — concurrent calls are serialised and the second through fifth calls will each be no-ops (migrationVersion already present after the first call completes).
- `deviceInfo` has no duplicate entries.
- `migrationVersion` has a single consistent value.



