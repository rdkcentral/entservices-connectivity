## 1. AC1 Migration Orchestration Setup

- [x] 1.1 Confirm migration declarations in BluetoothDeviceManager header are guarded by BLUETOOTH_ENABLE_AS_MIGRATION.
- [x] 1.2 Ensure AC1 helper signatures match adapter return semantics and init call needs.

## 2. Initialization Flow Implementation

- [x] 2.1 Implement PersistentStore-first check in init for device info retrieval.
- [x] 2.2 Ensure store-present path skips AS import and continues normal startup flow.
- [x] 2.3 Implement store-absent path to load AS data through adapter into cache.
- [x] 2.4 Persist imported cache into Bluetooth PersistentStore after successful AS import.

## 3. Non-Fatal Failure Policy

- [x] 3.1 Keep initialization non-fatal when AS source is missing.
- [x] 3.2 Keep initialization non-fatal when AS payload is invalid or parse fails.
- [x] 3.3 Ensure failure paths log migration outcome and continue reconciliation behavior.

## 4. Compile-Time Isolation

- [x] 4.1 Guard AC1 migration call sites in BluetoothDeviceManager implementation with BLUETOOTH_ENABLE_AS_MIGRATION.
- [x] 4.2 Verify OFF-mode build excludes AC1 migration symbols and behavior.

## 5. AC1 Validation Evidence

- [ ] 5.1 Validate scenario with preexisting PersistentStore data and capture output evidence.
- [ ] 5.2 Validate scenario with missing PersistentStore plus valid AS file and capture output evidence.
- [ ] 5.3 Validate scenario with missing PersistentStore plus malformed or missing AS file and capture graceful fallback evidence.
- [ ] 5.4 Attach AC1 implementation and validation evidence to CPESP-9452 tracking artifacts.
