# CPESP-9452 Implementation Checklist

## Purpose
This checklist converts AC1/AC2/AC3 into an implementation-ready, file-by-file execution plan.

## Scope
- Epic: CPESP-9452
- AC focus:
  - AC1 migration from filesystem persistence file to Bluetooth PersistentStore on init
  - AC2 rollback sync from Bluetooth persistence to filesystem persistence file on updates
  - AC3 compile-time feature-flag isolation for all AC1/AC2 code changes

## Tracking Legend
- [ ] not started
- [x] complete

## 1. Build And Feature Gating

### File: Bluetooth/CMakeLists.txt
- [ ] Add compile-time option covering all CPESP-9452 AC1+AC2 code (default ON): BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION.
- [ ] Export preprocessor define to target compile flags.
- [ ] Ensure option is visible and overridable by platform build config.

### File: Bluetooth/Bluetooth.config (if needed)
- [ ] Decide whether runtime config companion is needed (optional).
- [ ] If added, document interaction with compile-time flag and precedence.

Acceptance exit:
- [ ] Build with default settings compiles migration + rollback support paths.
- [ ] Build with flag OFF compiles without AC1/AC2 ticket code paths.

## 2. Add AS Persistence Adapter

### New file: Bluetooth/BluetoothPersistenceAdapter.h
- [ ] Define adapter interface for:
  - Read filesystem persistence file
  - Parse/validate payload shape
  - Map AS model <-> plugin model
  - Write filesystem persistence file atomically
- [ ] Guard adapter declarations/usages under AC3 feature-flag scope where applicable.

### New file: Bluetooth/BluetoothPersistenceAdapter.cpp
- [ ] Implement filesystem persistence file read from /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json.
- [ ] Implement write via temp-file + fsync + rename semantics.
- [ ] Implement schema-conformant serialization targeting docs/paired_bluetooth_devices.schema.json.
- [ ] Implement tolerant parse behavior for malformed/partial data (error surfaced to caller, no crash).
- [ ] Ensure adapter build inclusion is controlled by AC3 compile-time feature flag.

### File: docs/paired_bluetooth_devices.schema.json
- [ ] Confirm this file remains source-of-truth.
- [ ] Ensure adapter mapping aligns exactly with required fields.

Acceptance exit:
- [ ] Adapter unit-level behavior validated for read/write success and parse/write failures.

## 3. Implement AC1 Migration Path

### File: Bluetooth/BluetoothDeviceManager.h
- [ ] Add minimal declarations for migration orchestration and adapter usage.

### File: Bluetooth/BluetoothDeviceManager.cpp
- [ ] In init():
  - [ ] Check for Bluetooth PersistentStore data first.
  - [ ] If present, continue normal flow.
  - [ ] If absent, read filesystem persistence file through adapter.
  - [ ] Transform/import into plugin cache.
  - [ ] Persist imported data into Bluetooth PersistentStore.
- [ ] Keep init non-fatal on AS parse/read failure (log and continue with existing device reconcile behavior).
- [ ] Ensure entire AC1 migration path is isolated under AC3 compile-time feature flag.

Acceptance exit:
- [ ] AC1 behavior verified with and without preexisting PersistentStore data.
- [ ] AC1 code path is excluded from build/behavior when feature flag is OFF.

## 4. Implement AC2 Rollback Sync Path

### File: Bluetooth/BluetoothDeviceManager.cpp
- [ ] After successful updateStorageFromCache() write to PersistentStore, trigger filesystem persistence file sync via adapter when compile-time flag is ON.
- [ ] Ensure sync hook is invoked from all mutating paths:
  - [ ] init() path writes (migration/reconcile-triggered persistence updates)
  - [ ] setAutoConnect()
  - [ ] setLastConnectTimeUtc()
  - [ ] addDevice()
  - [ ] removeDevice()
  - [ ] any other path writing deviceInfo
- [ ] If AS write fails:
  - [ ] Log structured failure
  - [ ] Emit telemetry marker
  - [ ] Do not roll back PersistentStore update
- [ ] Ensure entire AC2 rollback path is isolated under the same AC3 compile-time feature flag.

Acceptance exit:
- [ ] AC2 behavior verified for flag ON and OFF.
- [ ] AC2 code path is excluded from build/behavior when feature flag is OFF.

## 6. Tests And Regression Coverage

### File: Tests/L1Tests/tests/test_Bluetooth.cpp
- [ ] Add tests for AC1:
  - [ ] Existing PersistentStore data: no AS import.
  - [ ] Missing PersistentStore + valid filesystem persistence file: import + persist.
  - [ ] Missing PersistentStore + missing filesystem persistence file: verify sync path writes filesystem persistence file during persistence updates.
  - [ ] Missing PersistentStore + malformed filesystem persistence file: graceful fallback.
- [ ] Add tests for AC2:
  - [ ] Mutations mirror to filesystem persistence file when flag ON.
  - [ ] Mutations do not mirror when flag OFF.
- [ ] Add AC3 coverage tests:
  - [ ] Migration and rollback helpers are not compiled/linked when flag OFF (or equivalent compile-time assertion target).
  - [ ] Baseline Bluetooth persistence behavior remains functional when flag OFF.
- [ ] Add upgrade/downgrade simulation test flow expectations where feasible in L1.

### File: docs/bluetooth-l1-tests.md (if maintained)
- [ ] Update test documentation for new migration/rollback scenarios.

Acceptance exit:
- [ ] L1 tests pass with flag ON.
- [ ] L1 tests pass with flag OFF.

## 7. Observability And Diagnostics

### File: Bluetooth/BluetoothDeviceManager.cpp
- [ ] Add structured logs for migration start/success/failure.
- [ ] Add structured logs for rollback sync success/failure.
- [ ] Add telemetry emission points for:
  - [ ] migration_attempted
  - [ ] migration_success
  - [ ] migration_failure
  - [ ] rollback_sync_success
  - [ ] rollback_sync_failure

Acceptance exit:
- [ ] Logs and telemetry are visible in validation runs and mapped to scenarios.

## 8. Non-Functional Safeguards

### Files: Bluetooth/BluetoothPersistenceAdapter.cpp, Bluetooth/BluetoothDeviceManager.cpp
- [ ] Ensure concurrency safety for AS-file writes.
- [ ] Avoid blocking boot-critical path beyond bounded file I/O.
- [ ] Ensure malformed AS data cannot crash plugin initialization.

Acceptance exit:
- [ ] Stress/repeat tests do not show corruption or race-induced failures.

## 9. Release And Deprecation Readiness

### Files: specs/bluetooth-plugin/open-questions.md, specs/bluetooth-plugin/design-notes.md
- [ ] Record resolved policy decisions in design/spec artifacts:
  - deviceAddr is Bluetooth MAC address
  - schema is source of truth for lastConnectionTimeUTC representation across both stores
  - BTMgr is authoritative for lastVolumeSetting
  - AS-file write failure handling is log+telemetry only
- [ ] Add deprecation trigger criteria for removing AC1+AC2 compile-time-gated ticket path.

Acceptance exit:
- [ ] Criteria for removing BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION path documented and approved.

## AC Traceability
- [ ] AC1 fully covered by checklist sections 2, 3, 5, 6, 7, 8.
- [ ] AC2 fully covered by checklist sections 1, 2, 4, 5, 6, 7, 8, 9.
- [ ] AC3 fully covered by checklist sections 1, 2, 3, 4, 6, 9.

## Final Definition Of Done
- [ ] All mandatory checklist items complete.
- [ ] AC1, AC2, and AC3 validation evidence attached to Jira CPESP-9452.
- [ ] No unresolved blocker in open questions for implementation merge.
