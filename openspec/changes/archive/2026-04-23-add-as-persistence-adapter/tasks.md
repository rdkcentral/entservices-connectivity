## 1. Adapter Interface And Build Wiring

- [x] 1.1 Add BluetoothAsPersistenceAdapter interface with read, parse, and atomic write responsibilities.
- [x] 1.2 Wire adapter compilation in Bluetooth/CMakeLists.txt under BLUETOOTH_ENABLE_AS_MIGRATION.
- [x] 1.3 Ensure adapter symbols and usages are excluded from OFF-mode builds.

## 2. Parse And Mapping Behavior

- [x] 2.1 Implement AS payload parsing for pairedDevices and required field extraction.
- [x] 2.2 Implement tolerant fallback coercion for lastConnectionTimeUTC numeric/string variants.
- [x] 2.3 Implement tolerant fallback coercion for autoConnectStatus boolean/numeric/string variants.
- [x] 2.4 Return explicit error codes for missing source, invalid payload, and write failures.

## 3. Atomic Write And Failure Handling

- [x] 3.1 Implement temp-file write and rename semantics for AS persistence updates.
- [x] 3.2 Preserve safe existing field values where plugin cache is unset or partial.
- [x] 3.3 Ensure write failures are surfaced without rolling back PersistentStore updates.

## 4. DeviceManager Integration

- [x] 4.1 Integrate adapter read path into migration flow when PersistentStore data is absent.
- [x] 4.2 Integrate adapter write sync hook after successful PersistentStore writes.
- [x] 4.3 Keep migration and rollback behavior non-fatal on AS read/parse errors.

## 5. L1 Validation Coverage

- [x] 5.1 Add L1 test for numeric timestamp plus string boolean coercion parse fallback.
- [x] 5.2 Add L1 test for string timestamp plus numeric boolean coercion parse fallback.
- [x] 5.3 Validate compile-time ON/OFF feature-gate behavior remains intact for adapter paths.

## 6. Verification And Evidence

- [ ] 6.1 Run L1 Bluetooth tests with migration flag ON and capture results.
- [ ] 6.2 Run L1 Bluetooth tests with migration flag OFF and capture results.
- [ ] 6.3 Attach implementation and validation evidence to CPESP-9452 tracking artifacts.
