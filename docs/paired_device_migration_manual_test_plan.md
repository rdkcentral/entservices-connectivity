# Bluetooth Migration & Rollback — Manual Test Plan

**Ticket:** RDKEMW-17454 / CPESP-9452  
**Feature Flag:** `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` (compile-time)  
**Persistent File:** `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`  
**PersistentStore Location:** namespace `Bluetooth`, key `deviceInfo`

---

## Prerequisites

| # | Item |
|---|------|
| P1 | Device running build with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION=ON` |
| P2 | Access to device shell (SSH/serial) |
| P3 | At least 2 Bluetooth audio devices available for pairing |
| P4 | "Old" firmware image (pre-migration, AS-managed persistence) available for rollback test |
| P5 | "New" firmware image (migration-enabled build) available |
| P6 | Ability to reboot device and flash firmware |

---

## Section 1: Migration (First Boot — No Prior PersistentStore Data)

### TC-MIG-01: Fresh migration from filesystem to PersistentStore

**Precondition:** Device has previously paired devices managed by AS Peripheral, stored in `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`. PersistentStore has NO `Bluetooth/deviceInfo` key (simulating first boot on new firmware).

**Steps:**
1. Confirm pre-existing file content:
   ```bash
   cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
   Note the devices listed (addresses, autoConnectStatus, lastConnectionTimeUTC).
2. Clear PersistentStore Bluetooth data (if any):
   ```bash
   # Via Thunder JSON-RPC:
   curl -d '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.deleteKey","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' http://localhost:9998/jsonrpc
   ```
3. Reboot the device.
4. Wait for Bluetooth plugin activation.
5. Verify PersistentStore now contains device info:
   ```bash
   curl -d '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' http://localhost:9998/jsonrpc
   ```
6. Verify previously-paired devices connect according to their `autoConnectStatus`.

**Expected Results:**
- PersistentStore `Bluetooth/deviceInfo` is populated with device entries matching those from the filesystem file.
- Device addresses, autoConnect settings, and lastConnectionTimeUTC values are preserved.
- Devices with `autoConnectStatus: true` auto-connect after boot.
- Filesystem file remains unchanged (it is read, not deleted).

---

### TC-MIG-02: Migration skipped when PersistentStore data already exists

**Precondition:** Device already has `Bluetooth/deviceInfo` in PersistentStore (i.e., migration was previously completed).

**Steps:**
1. Verify PersistentStore data exists:
   ```bash
   curl -d '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' http://localhost:9998/jsonrpc
   ```
2. Modify the filesystem file manually (add a fake device entry).
3. Reboot the device.
4. Check PersistentStore data again.

**Expected Results:**
- PersistentStore data is unchanged (migration is NOT re-run).
- The fake device entry added to the filesystem file is NOT imported.
- Log shows: "Migration attempted" is NOT logged (store data was found).

---

### TC-MIG-03: Migration graceful failure — missing filesystem file

**Precondition:** PersistentStore has no `Bluetooth/deviceInfo`. The filesystem file does NOT exist.

**Steps:**
1. Delete PersistentStore Bluetooth data.
2. Remove the filesystem file:
   ```bash
   rm /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
3. Reboot the device.
4. Confirm plugin activates successfully.

**Expected Results:**
- Plugin initializes without error.
- Log shows: "Migration skipped: Filesystem persistence source not found."
- New device pairs create fresh PersistentStore entries.

---

### TC-MIG-04: Migration graceful failure — malformed filesystem file

**Precondition:** PersistentStore has no `Bluetooth/deviceInfo`. Filesystem file contains invalid JSON.

**Steps:**
1. Delete PersistentStore Bluetooth data.
2. Write invalid content to the file:
   ```bash
   echo "NOT VALID JSON{{{" > /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
3. Reboot the device.

**Expected Results:**
- Plugin initializes without crashing.
- Log shows: "Migration failed: Filesystem persistence source is invalid"
- Device continues to operate normally; new pairings work.

---

## Section 2: Rollback Synchronization (Ongoing Filesystem Sync)

### TC-RB-01: New device pairing updates both PersistentStore AND filesystem

**Precondition:** Migration previously completed. Both stores in sync.

**Steps:**
1. Note current content of both stores.
2. Pair a new Bluetooth device.
3. Check PersistentStore:
   ```bash
   curl -d '{"jsonrpc":"2.0","id":1,"method":"org.rdk.PersistentStore.getValue","params":{"namespace":"Bluetooth","key":"deviceInfo"}}' http://localhost:9998/jsonrpc
   ```
4. Check filesystem file:
   ```bash
   cat /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```

**Expected Results:**
- New device appears in PersistentStore with `deviceID`, `deviceType`, and `autoconnect` fields.
- New device appears in filesystem file with `deviceAddr`, `friendlyName`, `deviceType`, `autoConnectStatus`, `lastVolumeSetting`, `lastConnectionTimeUTC` fields.
- Schema of filesystem file remains valid per `docs/paired_bluetooth_devices.schema.json`.

---

### TC-RB-02: AutoConnect change updates both stores

**Steps:**
1. Set autoConnect for a paired device:
   ```bash
   curl -d '{"jsonrpc":"2.0","id":1,"method":"org.rdk.Bluetooth.setAutoConnect","params":{"deviceID":"<ID>","enable":true}}' http://localhost:9998/jsonrpc
   ```
2. Verify PersistentStore reflects `autoconnect: 1`.
3. Verify filesystem file reflects `"autoConnectStatus": true` for that device address.

**Expected Results:**
- Both stores consistently reflect the autoConnect change.

---

### TC-RB-03: Device removal updates both stores

**Steps:**
1. Unpair a device.
2. Check PersistentStore — device entry should be removed.
3. Check filesystem file — device entry should be removed.

**Expected Results:**
- Device is absent from both stores.

---

### TC-RB-04: Filesystem sync failure does not roll back PersistentStore success

**Precondition:** Simulate filesystem write failure.

**Steps:**
1. Make the filesystem file read-only:
   ```bash
   chmod 444 /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
2. Pair a new device.
3. Check PersistentStore — new device should be present.
4. Check filesystem file — it should NOT have the new device (write blocked).
5. Restore permissions:
   ```bash
   chmod 644 /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```

**Expected Results:**
- PersistentStore write succeeds and new device is persisted there.
- Filesystem write fails silently (error logged).
- PersistentStore data is NOT rolled back.
- Log shows: "Filesystem persistence sync failed"

---

### TC-RB-05: HID devices excluded from filesystem sync

**Steps:**
1. Pair a Human Interface Device (e.g., BT keyboard/remote identified as "HUMAN INTERFACE DEVICE").
2. Verify it appears in PersistentStore.
3. Verify it does NOT appear in the filesystem file.

**Expected Results:**
- HID devices are in PersistentStore but excluded from the filesystem file (matching legacy AS behavior).

---

## Section 3: Firmware Rollback Scenario

### TC-ROLL-01: Full rollback — downgrade firmware and verify AS reads correct data

**Precondition:** Device running new firmware with migration complete. Several devices paired.

**Steps:**
1. Record current paired devices and their settings from both stores.
2. Pair 1–2 NEW devices on the new firmware (to ensure they get written to filesystem for rollback).
3. Verify both stores are in sync (PersistentStore and filesystem file contain all devices).
4. Flash/downgrade to the OLD firmware (pre-migration, AS-managed persistence).
5. Boot the device.
6. Verify AS Peripheral reads `/opt/persistent/sky/sky-asperipherals-bluetoothdevices.json`.
7. Verify all devices — including those newly paired on new firmware — are recognized and connect correctly.

**Expected Results:**
- Old firmware boots and finds all paired devices in the filesystem file.
- Devices auto-connect according to their saved `autoConnectStatus`.
- No device data is lost during the downgrade.

---

### TC-ROLL-02: Round-trip — upgrade → pair → downgrade → upgrade again

**Steps:**
1. Start on OLD firmware with 2 paired devices.
2. Upgrade to NEW firmware → migration should run (TC-MIG-01).
3. Pair 1 additional device on NEW firmware.
4. Downgrade to OLD firmware → verify all 3 devices work.
5. Pair 1 more device on OLD firmware.
6. Upgrade back to NEW firmware.
7. Verify migration does NOT re-run (PersistentStore exists from step 2).
8. Verify the device paired on old firmware (step 5) is picked up via BTRMGR reconciliation and added to both stores.

**Expected Results:**
- No data loss at any transition point.
- Migration runs only once (first upgrade).
- Subsequent upgrades skip migration because PersistentStore already has data.
- Devices paired on old firmware are reconciled into the cache via BTRMGR on new firmware init.

---

## Section 4: Reboot Persistence Validation

### TC-REBOOT-01: PersistentStore and filesystem survive reboot

**Steps:**
1. Pair devices, set autoConnect, verify settings.
2. Reboot device.
3. Verify PersistentStore contains expected data.
4. Verify filesystem file contains expected data.
5. Verify devices auto-connect per settings.

**Expected Results:**
- All data survives reboot.
- autoConnect and lastConnectionTimeUTC are preserved.

---

## Section 5: Edge Cases

### TC-EDGE-01: Empty pairedDevices array in filesystem

**Steps:**
1. Clear PersistentStore.
2. Write valid but empty file:
   ```bash
   echo '{"pairedDevices":[]}' > /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json
   ```
3. Reboot.

**Expected Results:**
- Migration completes with zero devices imported.
- Plugin continues normally.

---

### TC-EDGE-03: Device in filesystem but no longer paired in BTRMGR

**Steps:**
1. Filesystem file has a device entry with address "AA:BB:CC:DD:EE:FF".
2. That device is NOT in the platform's paired device list.
3. Run migration.

**Expected Results:**
- Device is skipped during import (no matching BTRMGR handle).
- Log shows: "No paired device handle found for addr=AA:BB:CC:DD:EE:FF during filesystem persistence import, skipping"
- Other devices import normally.

---

## Log Verification Checklist

During all test execution, verify these log patterns in device logs:

| Scenario | Expected Log |
|----------|-------------|
| Successful migration | "Migration succeeded: Filesystem persistence data imported and persisted to PersistentStore" |
| Migration skipped (store exists) | No "Migration attempted" log |
| No filesystem file | "Migration skipped: Filesystem persistence source not found" |
| Invalid filesystem file | "Migration failed: Filesystem persistence source is invalid" |
| Filesystem sync success | "Filesystem persistence sync succeeded: Persistence payload updated from cache" |
| Filesystem sync skipped (no change) | "Filesystem persistence sync skipped: cache unchanged since last write" |
| Filesystem sync failure | "Filesystem persistence sync failed" |
