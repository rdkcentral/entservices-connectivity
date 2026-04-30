## Why

AC2 rollback synchronization is a distinct CPESP-9452 requirement and needs explicit implementation and validation scope separate from AC1 migration initialization logic. Scoping AC2 now enables deterministic verification that PersistentStore writes are mirrored to the AS file under feature gating without destabilizing baseline persistence behavior.

## What Changes

- Implement AC2 rollback sync hook after successful Bluetooth PersistentStore writes.
- Ensure sync hook covers all mutating paths that write deviceInfo payloads.
- Keep AS-sync failures non-transactional: log and emit telemetry without rolling back PersistentStore updates.
- Enforce compile-time isolation of AC2 rollback behavior under BLUETOOTH_ENABLE_AS_MIGRATION.
- Add AC2-focused validation and evidence capture tasks for ON and OFF flag behavior.

## Capabilities

### New Capabilities
- ac2-rollback-sync-path: Defines post-persist rollback synchronization from Bluetooth PersistentStore model to AS file with non-rollback failure policy.

### Modified Capabilities
- None.

## Impact

- Affected code: Bluetooth/BluetoothDeviceManager.cpp and adapter integration points.
- Behavioral impact: every successful deviceInfo persistence update can trigger AS sync when compile-time flag is enabled.
- Observability impact: requires explicit rollback sync success/failure evidence and policy-conformant logging behavior.
