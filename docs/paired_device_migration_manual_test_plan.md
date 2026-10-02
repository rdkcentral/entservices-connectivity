# Bluetooth Migration & Rollback — Manual Test Plan

**Ticket:** RDKEMW-20057 / CPESP-9979
**Feature Flag:** `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` (compile-time)  
**AS Persistent File:** `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`  
**PersistentStore Location:** namespace `Bluetooth`, key `deviceInfo`  
**PersistentStore Migration Version Key:** namespace `Bluetooth`, key `migrationVersion`

---

## Overview

Migration is **client-triggered**, not automatic. The plugin does **not** auto-migrate at init. The IUI/AS client is responsible for calling the appropriate API on first boot. Since no client (IUI/AS) currently calls these APIs, the tester simulates client behaviour directly via curl.

### API Summary

| API | Method | Description |
|-----|--------|-------------|
| `performMigration` | `org.rdk.Bluetooth.1.performMigration` | **First call:** imports AS file → RDK PersistentStore, writes `migrationVersion=1`. **Subsequent calls:** no-op (migrationVersion already present). |
| `clearMigration` | `org.rdk.Bluetooth.1.clearMigration` | Deletes `deviceInfo` and `migrationVersion` from RDK PersistentStore, clears the plugin's paired device cache, and resets migration state. AS file is **not** touched. |

### Migration State

The plugin tracks whether migration has been performed. Migration state becomes active after a successful `performMigration` call and is cleared by `clearMigration`. Key observable effects:

- `setAutoConnect` is **rejected** (returns `{"result":{"success":false}}` in the response body) when migration has not been performed.
- `setAutoConnect` changes will **not** be written to PersistentStore when migration has not been performed.
- Pairing and unpairing (`addDevice`/`removeDevice`) update the in-memory device cache but do **not** write to PersistentStore when migration has not been performed.
- The AS file (filesystem persistence) will **not** be updated by any operation external to IUI/AS (e.g. CURL'd plug-in requests) — pairing, unpairing, or `setAutoConnect` — when migration has not been performed.
- `getAutoConnect` has a migration guard, but instead of rejecting, it returns `autoconnect: false` (success) for any deviceID when migration has not been performed — it bypasses the cache lookup entirely and returns disabled without error.

### Curl Reference Commands

```bash
# Invoke performMigration (simulating IUI "LD Enabled" first-boot behaviour)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# Invoke clearMigration (simulating IUI "LD Disabled" / rollback behaviour)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
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
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
  http://127.0.0.1:9998/jsonrpc

# Get autoConnect for a device
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.getAutoConnect","params":{"deviceID":"<ID>"}}' \
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

`setAutoConnect` will be **rejected** (returns `{"result":{"success":false}}` in the response body) until a successful `performMigration` has occurred since the last `clearMigration`, or equivalently, until the `migrationVersion` key is present in PersistentStore. Because the plugin re-derives migration state from that key at init, migration remains enabled across reboots without needing `performMigration` to be called again.

---

## Section 1: performMigration — Initial Migration (Happy Path)

> **State entering this section:** AS file exists and contains paired device entries. Both `deviceInfo` and `migrationVersion` are absent from PersistentStore. The TC-MIG-01 **Setup** block immediately below establishes this clean state before testing begins.

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

# Confirm pre-existing AS file and record its checksum and mtime for later comparison
cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
ls -la /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
```
Note the device addresses, `autoConnectStatus`, and `lastConnectionTimeUTC` values listed in the file. Also record the md5sum output and the last-modified timestamp (`ls -la`) — these baseline values are required for the AS file integrity check in step 6.

**Steps:**
1. Reboot device and wait for Bluetooth plugin activation.
2. Verify `migrationVersion` is absent from PS. When the migration-enabled build runs `init` with no valid `migrationVersion` present, the cache is left empty and no data is written to PersistentStore — `deviceInfo` will also be absent (or contain only stale data from a prior firmware, not from this boot):
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
     http://127.0.0.1:9998/jsonrpc
   # Expected: JSON-RPC error response (key not found)
   ```
3. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Verify `deviceInfo` is now populated in PersistentStore with data imported from the AS file:
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
6. Verify the AS file is **unchanged** after `performMigration`. The filesystem sync (`writeFilesystemPersistenceFromCache`) is guarded by `_isMigrated`, which is only set to `true` after the PersistentStore write completes — so the AS file is not touched during `performMigration`. Byte equality and mtime checks **are** appropriate here:
   ```bash
   md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ls -la /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
   Confirm checksum and mtime are identical to the values recorded in Setup.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `Bluetooth/deviceInfo` is populated with device entries matching those from the AS file.
- `Bluetooth/migrationVersion` is set to `"1"`.
- Device `autoConnectStatus` and `lastConnectionTimeUTC` values from the AS file are preserved in PS.
- The AS file is **not** modified by `performMigration` — its checksum and mtime are unchanged.

**Expected Log Entries:**
- `migration_attempted`
- `initial migration succeeded`

---

### TC-MIG-02: performMigration is a no-op when migrationVersion is already present

**Precondition:** TC-MIG-01 has been completed successfully. Both `deviceInfo` and `migrationVersion` are present in PS.

**Steps:**
1. Record the current `migrationVersion` value.
2. Record the current `deviceInfo` value.
3. Call `performMigration` again **without modifying the AS file**:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Read `deviceInfo` and `migrationVersion` again and compare to values from step 1–2.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `deviceInfo` is **unchanged**.
- `migrationVersion` is **unchanged**.
- No new import occurs.

**Expected Log Entries:**
- `migration_attempted`
- `migration already completed (migrationVersion=1 present), no sync needed`

---

### TC-MIG-03: performMigration is always a no-op after first migration (migrationVersion present)

> **Note:** After the first successful `performMigration`, subsequent calls are always no-ops regardless of AS file content.

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
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Read `deviceInfo` and `migrationVersion` from PS.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `migrationVersion` is **unchanged** (`"1"`) — no re-import occurs.
- `deviceInfo` is **unchanged** — the modified AS file is ignored.

**Expected Log Entries:**
- `migration_attempted`
- `migration already completed (migrationVersion=1 present), no sync needed`

---

## Section 2: clearMigration — Rollback

> **State entering this section:** Both `deviceInfo` and `migrationVersion` are present in PS (migration is active from TC-MIG-01). The AS file is present (possibly with the additional device entry manually added in TC-MIG-03).

### TC-CLR-03: clearMigration then performMigration re-migrates successfully

**Precondition:** TC-MIG-01 through TC-MIG-03 have completed. Migration is active — both `deviceInfo` and `migrationVersion` are present in PS. The AS file is present.

**Steps:**
1. Call `clearMigration` to reset state.
2. Verify both PS keys are absent.
3. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
4. Verify `deviceInfo` and `migrationVersion` are present in PS again.
5. Verify device data matches the AS file.

**Expected Results:**
- After `clearMigration`, both keys are absent.
- After `performMigration`, both keys are restored with data from the AS file.
- `performMigration` returns `{"success": true}`.

**Expected Log Entries (step 3):**
- `migration_attempted`
- `initial migration succeeded` (treated as first time since migrationVersion key was absent)

---

### TC-CLR-01: clearMigration wipes PersistentStore and resets migration state

**Precondition:** TC-CLR-03 has completed. `performMigration` was called in TC-CLR-03 step 3; both `deviceInfo` and `migrationVersion` are present in PS. The AS file is present.

**Steps:**
1. Confirm both PS keys exist (see Curl Reference Commands). Record the current AS file content and checksum for the unchanged check in step 5:
   ```bash
   md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ls -la /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
2. Call `clearMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
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
5. Verify the AS file is **unchanged** by comparing checksum and mtime to the values recorded in step 1:
   ```bash
   md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ls -la /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```

**Expected Results:**
- `clearMigration` returns `{"success": true}`.
- `Bluetooth/deviceInfo` is absent from PS.
- `Bluetooth/migrationVersion` is absent from PS.
- AS file checksum and mtime are identical to the values recorded in step 1.
- `getPairedDevices` will return expected entries but with `autoconnect` fields set to `false`.

**Expected Log Entries:**
- `PersistentStore cleared and migration state reset`

---

### TC-CLR-02: clearMigration on already-empty PersistentStore (idempotent)

**Precondition:** TC-CLR-01 has completed. Neither `deviceInfo` nor `migrationVersion` exist in PS.

**Steps:**
1. Confirm both PS keys are absent.
2. Call `clearMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- `clearMigration` returns `{"success": true}` — deleting absent keys is not an error.

**Expected Log Entries:**
- `PersistentStore cleared and migration state reset`

---

## Section 3: Pre-Migration Guard on setAutoConnect

> **State entering this section:** PS is empty and migration state is `false` (from TC-CLR-02). The `clearMigration` call in TC-GUARD-01's Setup is a no-op confirmation of this state.

### TC-GUARD-01: setAutoConnect rejected before performMigration

**Precondition:** Migration has not been performed — either the device has never had `performMigration` called, or `clearMigration` has been called since the last `performMigration`. Verify this by confirming `migrationVersion` is absent from PersistentStore.

**Setup (confirm pre-migration state — this is a no-op from TC-CLR-02, which already left PS empty):**
```bash
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
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
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- Response returns `{"result":{"success":false}}`.
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
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Call `setAutoConnect` with the same device ID from TC-GUARD-01:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
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
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Attempt `setAutoConnect` on the same device:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.setAutoConnect","params":{"deviceID":"<ID>","enable":false}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- `setAutoConnect` returns `{"result":{"success":false}}`.
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
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.getAutoConnect","params":{"deviceID":"<ID>"}}' \
     http://127.0.0.1:9998/jsonrpc
   ```

**Expected Results:**
- `getAutoConnect` DOES have a migration state check. When `_isMigrated=false`, the code immediately returns `AUTO_CONNECT_STATUS_DISABLED` with success — it bypasses the cache lookup entirely. No device-not-found error is returned.
- Response: `{"success": true, "autoconnect": false}` for any `deviceID` when migration has not been performed, regardless of whether the device exists in the (empty) cache.
- This distinguishes it from `setAutoConnect`: `setAutoConnect` explicitly rejects with an error when not migrated; `getAutoConnect` silently returns disabled.

---

## Section 4: Rollback Synchronization (Ongoing Filesystem Sync)

> **State entering this section:** Migration state is `false` and PS is empty (clearMigration was last called in TC-GUARD-03). The AS file exists with known content.

### TC-RB-05: AS file is NOT modified when performMigration has never been called

**Precondition:** Continuing from TC-GUARD-04: migration state is `false` and PS is empty (clearMigration was last called in TC-GUARD-03). The AS file exists with known content. The `clearMigration` call in Setup confirms this state.

**Setup:**
```bash
# Confirm pre-migration state (clearMigration is a no-op here; PS is already empty from TC-GUARD-03)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# Record a checksum of the AS file before the test
md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
# OR note the last-modified timestamp
ls -la /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
```

**Steps:**
1. Pair a new Bluetooth audio device.
2. Verify the in-memory cache IS updated (the device appears in `getPairedDevices`), but verify PS `deviceInfo` is **not** written by the pairing — `addDevice` skips the PersistentStore write when `_isMigrated=false`:
   ```bash
   # Confirm getPairedDevices shows the newly paired device (in-memory cache was updated)
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.getPairedDevices"}' \
     http://127.0.0.1:9998/jsonrpc
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
- PersistentStore `deviceInfo` is **not** updated on pairing or unpairing. While `addDevice`/`removeDevice` update the in-memory device cache, the PersistentStore write is skipped when `_isMigrated=false`.
- The AS file is **not** modified at any point — the filesystem sync is also guarded by migration state.
- AS file checksum and mtime are identical to the baseline recorded before step 1.

**Expected Log Entries** (no migration-specific skip log exists for `writeStorageFromCache`; search device logs for the absence of the filesystem sync success message):
- Confirm that `Filesystem persistence sync succeeded` does **not** appear in device logs during this test (the AS file write is skipped silently when `_isMigrated` is false).

---

> **Section Transition — establish migration state before TC-RB-01:**
> TC-RB-05 leaves migration state as `false`. The tests TC-RB-01 through TC-RB-06 require migration to be active. Call `performMigration` once now before proceeding:
> ```bash
> curl --header "Content-Type: application/json" --request POST \
>   --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
>   http://127.0.0.1:9998/jsonrpc
> ```
> Verify that `migrationVersion` is present in PS before continuing to TC-RB-01.

### TC-RB-01: New device pairing updates both PersistentStore AND filesystem

**Precondition:** `performMigration` was called in the transition step above. Both stores are in sync.

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
     --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.1.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' \
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

### TC-RB-06: AS file is NOT modified after clearMigration (until performMigration is called again)

**Precondition:** TC-RB-04 has completed. Migration is active (`performMigration` was called in TC-RB-04 step 1 or in the section-transition step before TC-RB-01). Both PS and the AS file are in sync. Ensure at least one device is paired with a known `autoConnectStatus` (configured in TC-RB-02; if TC-RB-03 removed that device, use `setAutoConnect` to configure a currently paired device).

**Setup:**
```bash
# Record a checksum of the AS file before the test
md5sum /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
```

**Steps:**
1. Call `clearMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
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
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
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

## Section 5: performMigration — Edge Cases (Destructive AS File Operations)

> **State entering this section:** Migration is active after TC-RB-06 (step 7 called `performMigration`). The following tests modify or delete the AS persistence file. Each test includes a Setup block that clears PS state and prepares the AS file before the test steps run.

### TC-MIG-04: performMigration with empty AS file

**Precondition:** Both `deviceInfo` and `migrationVersion` must be absent from PS, and the AS file must contain only an empty device list.

**Setup:**
```bash
# 1. Clear PS state from TC-RB-06 (migration is currently active)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# 2. Write an empty AS file
echo '{"pairedDevices":[]}' > /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json

# 3. Confirm both PS keys are now absent
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc
# Expected: error response (key not found)
```

**Steps:**
1. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Read `deviceInfo` from PS.
3. Read `migrationVersion` from PS.

**Expected Results:**
- `performMigration` returns `{"success": true}`.
- `deviceInfo` is present but contains an empty array (`[]`).
- `migrationVersion` is present and set to `"1"`.
- Plugin continues operating normally.

**Expected Log Entries:**
- `migration_attempted`
- `imported cache is empty, skipping BTRMGR enrichment`
- `initial migration succeeded`

---

### TC-MIG-05: performMigration when AS file does not exist

**Precondition:** Both `deviceInfo` and `migrationVersion` must be absent from PS, and the AS file must not exist.

**Setup:**
```bash
# 1. Clear PS state from TC-MIG-04 (both PS keys are present from that test)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# 2. Delete the AS file
rm -f /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json

# 3. Confirm both PS keys are now absent
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc
# Expected: error response (key not found)
```

**Steps:**
1. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Read `deviceInfo` from PS.
3. Read `migrationVersion` from PS.

**Expected Results:**
- `performMigration` returns `{"success": true}` (absent file treated as empty, not an error).
- `deviceInfo` is present and contains an empty or minimal array.
- `migrationVersion` is present and set to `"1"`.

**Expected Log Entries:**
- `migration_attempted`
- `filesystem persistence file does not exist: /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json` (logged by the persistence adapter when the file is not found; absent file is treated as empty — no error is returned)
- `imported cache is empty, skipping BTRMGR enrichment`
- `initial migration succeeded`

---

## Section 6: Edge Cases

> **State entering this section:** After TC-MIG-05, the AS file does not exist and both PS keys are present (from TC-MIG-05's `performMigration`). TC-EDGE-01 includes a Setup block to restore the required state.

### TC-EDGE-01: Device in AS file but no longer paired in BTRMGR

**Precondition:** Both PS keys must be absent and the AS file must contain a device entry whose address is not in the platform's paired device list.

**Setup:**
```bash
# 1. Clear PS state from TC-MIG-05 (both PS keys are present from that test)
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# 2. Create a test AS file containing one non-paired device address.
#    Replace AA:BB:CC:DD:EE:FF with a MAC address that is NOT currently in the platform paired list.
cat > /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json << 'EOF'
{
  "pairedDevices": [
    {
      "deviceAddr": "AA:BB:CC:DD:EE:FF",
      "friendlyName": "Phantom Device",
      "deviceType": "AUDIO OUTPUT",
      "autoConnectStatus": false,
      "lastVolumeSetting": 50,
      "lastConnectionTimeUTC": "2024-01-01T00:00:00Z"
    }
  ]
}
EOF

# 3. Confirm both PS keys are now absent
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc
# Expected: error response (key not found)
```

**Steps:**
1. Call `performMigration`:
   ```bash
   curl --header "Content-Type: application/json" --request POST \
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
2. Read `deviceInfo` from PS.

**Expected Results:**
- Device with address `AA:BB:CC:DD:EE:FF` is skipped during import because it is no longer in the platform's paired device list.
- Other valid devices import normally.
- `performMigration` returns `{"success": true}`.

**Expected Log Entries:**
- `migration_attempted`
- `No paired device handle found for addr=AA:BB:CC:DD:EE:FF during filesystem persistence import, skipping`
- `initial migration succeeded`

---

### TC-EDGE-02: performMigration called multiple times concurrently (stress)

**Precondition:** `migrationVersion` must be absent from PS. TC-EDGE-01 left both PS keys present (from its `performMigration` call). Call `clearMigration` in Setup to reset state.

**Setup:**
```bash
# Clear PS state from TC-EDGE-01
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# Confirm migrationVersion is absent
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc
# Expected: error response (key not found)
```

**Steps:**
1. Fire multiple concurrent `performMigration` calls in quick succession:
   ```bash
   for i in {1..5}; do
     curl --header "Content-Type: application/json" --request POST \
       --data '{"jsonrpc":"2.0","id":'"$i"',"method":"org.rdk.Bluetooth.1.performMigration","params":{}}' \
       http://127.0.0.1:9998/jsonrpc &
   done
   wait
   ```
2. Verify `deviceInfo` and `migrationVersion` are present and contain valid data.
3. Verify no duplicate device entries in `deviceInfo`.

**Expected Results:**
- All calls return `{"success": true}`.
- Only one import occurs — concurrent calls are serialised and the second through fifth calls will each be no-ops (migrationVersion already present after the first call completes).
- `deviceInfo` has no duplicate entries.
- `migrationVersion` has a single consistent value.

---

## Section 7: Reboot Persistence Validation

> **State entering this section:** After TC-EDGE-02, migration is active (`migrationVersion` is present in PS). Ensure at least one real Bluetooth audio device is paired with a known `autoConnect` setting before TC-REBOOT-01. If edge-case testing left only a phantom device in the AS file, re-pair a real device and configure its `autoConnect` state via `setAutoConnect` before proceeding.

### TC-REBOOT-01: PersistentStore and filesystem survive reboot after migration

**Precondition:** TC-EDGE-02 has completed; migration is active (`migrationVersion` is present in PS). At least one real Bluetooth audio device is paired with `autoConnect` configured via `setAutoConnect`. If no real device is currently paired (edge-case tests may have left only a phantom device in the AS file), re-pair a real device and call `setAutoConnect` to configure its `autoConnect` state before rebooting.

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
- `Filesystem persistence sync succeeded: Persistence payload updated from cache, cache_size=N` (emitted only when a persistence mutation triggers `writeStorageFromCache()` after boot; it may not appear during initialization)

---

### TC-REBOOT-02: setAutoConnect rejected after reboot when migration was never performed

**Precondition:** TC-REBOOT-01 has completed. Migration is active (`migrationVersion` is present in PS). Call `clearMigration` to clear PS state before rebooting.

**Setup:**
```bash
# 1. Clear PS state from TC-REBOOT-01
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
  http://127.0.0.1:9998/jsonrpc

# 2. Confirm both PS keys are absent before rebooting
curl --header "Content-Type: application/json" --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"migrationVersion"}}' \
  http://127.0.0.1:9998/jsonrpc
# Expected: error response (key not found)
```

**Steps:**
1. Reboot the device (`clearMigration` was already called in Setup).
2. After boot, attempt `setAutoConnect` for any paired device **before** calling `performMigration`.

**Expected Results:**
- `setAutoConnect` returns `{"result":{"success":false}}`.
- The plugin correctly identifies on boot that no prior migration has been performed (no migrationVersion key in PersistentStore) and enforces the guard.

**Expected Log Entries on boot** (search device logs for this exact string):
- `Migration state at init: _isMigrated=false`

---

## Section 8: Firmware Rollback Scenario

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
     --data '{"jsonrpc":"2.0","id":42,"method":"org.rdk.Bluetooth.1.clearMigration","params":{}}' \
     http://127.0.0.1:9998/jsonrpc
   ```
3. Verify PS `deviceInfo` and `migrationVersion` are gone.
4. Verify the AS file is **unchanged** (clearing only affects PS, not the AS file).
5. Attempt `setAutoConnect` for any device — verify it returns `{"result":{"success":false}}`.
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
- `migration_attempted`
- `initial migration succeeded`

