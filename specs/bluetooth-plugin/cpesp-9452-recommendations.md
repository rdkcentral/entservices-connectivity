# CPESP-9452 Explore Recommendations

## Epic
- Key: CPESP-9452
- Summary: legacy bluetooth peripheral <-> Bluetooth Plugin: Migration and Rollback Support
- Source AC field: customfield_11643

## Acceptance Criteria Interpreted

### AC1 Migration
- On Bluetooth plugin initialization, plugin SHALL check for its own PersistentStore data first.
- If present, continue normal initialization.
- Else, read /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json, transform to plugin format, and write into PersistentStore.

### AC2 Rollback
- Whenever Bluetooth plugin updates its own persistent data, plugin SHALL also update filesystem persistence file at /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json after data conversion.
- This applies to all persistent-data update paths, including initialization-time migration/reconcile writes and runtime mutation writes.

### AC3 Feature Flag Support
- Code changes covered by AC1 and AC2 SHALL be isolated under a compile-time feature flag.
- The feature flag SHALL be enabled by default.
- Recommended compile-time option: BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION.
- When migration/rollback support is no longer required, all AC1/AC2 code introduced under this ticket SHALL be removable from build via this flag.

## Current State Gap Analysis

1. No migration reader for filesystem persistence file
- No code path currently reads /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json in [Bluetooth/BluetoothDeviceManager.cpp](Bluetooth/BluetoothDeviceManager.cpp).

2. No rollback writer for filesystem persistence file
- No code path currently writes /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json in [Bluetooth/BluetoothDeviceManager.cpp](Bluetooth/BluetoothDeviceManager.cpp).

3. Persistent schema mismatch risk
- Current runtime persistence model in [Bluetooth/BluetoothDeviceManager.cpp](Bluetooth/BluetoothDeviceManager.cpp) uses deviceID/autoconnect/lastConnectTimeUtc.
- Current source-of-truth schema in [docs/paired_bluetooth_devices.schema.json](docs/paired_bluetooth_devices.schema.json) requires pairedDevices wrapper and fields deviceAddr/friendlyName/deviceType/lastVolumeSetting/autoConnectStatus/lastConnectionTimeUTC.
- Migration/rollback implementation must include explicit field mapping and compatibility handling.
- PersistentStore format should be aligned to [docs/paired_bluetooth_devices.schema.json](docs/paired_bluetooth_devices.schema.json) for this unreleased software.

4. No unified compile-time feature switch for all CPESP-9452 code paths
- No dedicated compile-time guard that covers both AC1 migration flow and AC2 rollback synchronization in [Bluetooth/CMakeLists.txt](Bluetooth/CMakeLists.txt), [Bluetooth/BluetoothDeviceManager.h](Bluetooth/BluetoothDeviceManager.h), or [Bluetooth/BluetoothDeviceManager.cpp](Bluetooth/BluetoothDeviceManager.cpp).

## Recommended Changes Required To Satisfy AC

## 1) Add AS Persistence Adapter Layer
Create an isolated adapter in Bluetooth module:
- New files:
  - [Bluetooth/BluetoothPersistenceAdapter.h](Bluetooth/BluetoothPersistenceAdapter.h)
  - [Bluetooth/BluetoothPersistenceAdapter.cpp](Bluetooth/BluetoothPersistenceAdapter.cpp)

Responsibilities:
- Read/parse filesystem persistence file from /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json.
- Validate against [docs/paired_bluetooth_devices.schema.json](docs/paired_bluetooth_devices.schema.json).
- Convert between AS schema model and plugin internal model.
- Write filesystem persistence file atomically (temp file + rename) with lock/serialization.

Rationale:
- Keeps migration/rollback logic isolated and removable later.
- Prevents schema mapping logic from spreading across manager methods.

## 2) Implement AC1 Migration During Init
Update initialization flow in [Bluetooth/BluetoothDeviceManager.cpp](Bluetooth/BluetoothDeviceManager.cpp):
- Step A: attempt load from PersistentStore.
- Step B: if not found or empty, read filesystem persistence file through adapter.
- Step C: transform and populate cache.
- Step D: persist transformed cache to PersistentStore.
- Step E: continue existing reconcile-with-device flow.

Required behavior details:
- Treat malformed AS JSON as non-fatal: log error and continue with current-device discovery flow.
- Preserve boot reliability: initialization must not fail solely due to filesystem persistence file parse failure.

## 3) Implement AC2 Rollback Synchronization Hooks
After every successful PersistentStore update, mirror to filesystem persistence file when BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is enabled.

Hook points in [Bluetooth/BluetoothDeviceManager.cpp](Bluetooth/BluetoothDeviceManager.cpp):
- updateStorageFromCache()
- setAutoConnect()
- setLastConnectTimeUtc()
- addDevice()
- removeDevice()
- Any future path mutating persisted metadata

Coverage note:
- Because AC2 is phrased as every persistent-data update, sync must also occur for writes triggered during init migration/reconcile flows when those flows persist plugin data, while BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is enabled.

Write policy:
- Update filesystem persistence file only after PersistentStore write succeeds.
- If AS-file write fails: log error and emit telemetry marker; do not roll back PersistentStore write.
- No explicit API/status event error propagation is required for AS-file write failure.

## 4) Add Compile-Time Feature Flag (Enabled By Default, AC1+AC2 Scope)
Add CMake option in [Bluetooth/CMakeLists.txt](Bluetooth/CMakeLists.txt):
- Example: BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION default ON

Add define to target compile options and wrap all CPESP-9452 code paths:
- AC1 migration read/transform/import path
- AC2 rollback mirror-write path
- Adapter wiring and helper logic that exists only for migration/rollback support

Design constraint:
- Feature flag must isolate all ticket code changes required for migration/rollback.
- Feature flag OFF build should exclude AC1/AC2 code introduced by this ticket while preserving baseline Bluetooth behavior.
- Design must minimize "peppering" of code with flag checks. If necessary, define a code structure that helps avoid this.

## 6) Validation And Regression Expansion
Add/expand tests for migration and rollback behavior:
- [Tests/L1Tests/tests/test_Bluetooth.cpp](Tests/L1Tests/tests/test_Bluetooth.cpp)

Required new test groups:
1. Init with existing PersistentStore data: no AS-file import path executed.
2. Init without PersistentStore data and valid filesystem persistence file: import executed and persisted.
3. Init without PersistentStore data and missing filesystem persistence file: Verify filesystem persistence file is still written to when sync'ing persistent store and local file.
4. Init without PersistentStore data and malformed filesystem persistence file: graceful fallback.
5. Update operations mirror to filesystem persistence file when flag ON.
6. Update operations do not mirror when flag OFF.
7. Upgrade/downgrade simulation preserving paired devices/autoconnect/last-connect data.
8. Init migration path is not compiled/executed when flag OFF.

## 7) Operational Hardening
- Add structured logs for migration start/success/failure and rollback sync result.
- Add telemetry markers for:
  - migration attempted
  - migration success/failure
  - rollback sync success/failure
- Guard against concurrent writes via lock or serialized executor.

## Proposed Delivery Phasing

Phase 1
- Add unified compile-time flag for AC1+AC2 scope.
- Add adapter layer and mapping table (under flag scope).
- Implement AC1 migration import path (under flag scope).
- Add baseline tests for migration.

Phase 2
- Implement AC2 rollback sync hooks (under same flag scope).
- Add rollback and flag behavior tests.

Phase 3
- Expand upgrade/downgrade regression tests and telemetry verification.
- Finalize deprecation plan for BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION removal.

## Resolved Decisions

1. deviceAddr identity
- deviceAddr is always the Bluetooth device MAC address.

2. lastConnectionTimeUTC format
- Use the representation defined in [docs/paired_bluetooth_devices.schema.json](docs/paired_bluetooth_devices.schema.json).
- Use one consistent format across both local file and PersistentStore.
- The schema file is the source of truth.

3. lastVolumeSetting ownership
- BTMgr is authoritative for lastVolumeSetting.

4. failure policy
- log + telemetry is sufficient for AS-file write failure handling.
- No explicit status event/API error detail is required.

## AC Traceability Matrix

- AC1 satisfied by recommendations 1, 2, 5, 6.
- AC2 satisfied by recommendations 1, 3, 4, 5, 6.
- AC3 satisfied by recommendations 2, 3, 4, 6.
- Upgrade/downgrade validation guidance covered by recommendations 6 and delivery phase 3.
