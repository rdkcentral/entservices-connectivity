## Why

AC1 migration behavior is a discrete requirement in CPESP-9452 and needs a focused implementation plan that can be validated independently from broader AC2 rollback work. Isolating this scope now clarifies ownership of Bluetooth initialization migration behavior and reduces merge risk for remaining checklist sections.

## What Changes

- Implement AC1 migration logic in BluetoothDeviceManager initialization flow.
- Ensure PersistentStore-first behavior with AS-file import only when Bluetooth store data is absent.
- Keep migration path non-fatal on AS read or parse failures and continue normal device reconciliation.
- Enforce compile-time isolation of AC1 migration logic under BLUETOOTH_ENABLE_AS_MIGRATION.
- Add AC1-focused verification tasks and evidence capture for preexisting and missing PersistentStore scenarios.

## Capabilities

### New Capabilities
- ac1-migration-path: Defines initialization-time migration behavior from AS file to Bluetooth PersistentStore with graceful fallback.

### Modified Capabilities
- None.

## Impact

- Affected code: Bluetooth/BluetoothDeviceManager.h and Bluetooth/BluetoothDeviceManager.cpp.
- Behavioral impact: initialization flow chooses migration path only when store data is absent and migration feature is compiled in.
- Validation impact: requires targeted AC1 scenario evidence for both store-present and store-absent paths.
