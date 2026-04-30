## 1. AC1 Regression Coverage in L1 Tests

- [x] 1.1 Add L1 test for PersistentStore-present initialization path that verifies no AS import is performed.
- [x] 1.2 Add L1 test for missing PersistentStore plus valid AS payload resulting in import and persistence.
- [x] 1.3 Add L1 test for missing PersistentStore plus missing AS source ensuring graceful fallback behavior.
- [x] 1.4 Add L1 test for missing PersistentStore plus malformed AS payload ensuring non-fatal initialization.

## 2. AC2 and AC3 Feature-Flag Validation

- [x] 2.1 Add L1 tests validating mutating paths mirror to AS output when BLUETOOTH_ENABLE_AS_MIGRATION is ON.
- [x] 2.2 Add L1 tests validating mutating paths do not mirror to AS output when BLUETOOTH_ENABLE_AS_MIGRATION is OFF.
- [x] 2.3 Add compile-gating coverage assertions demonstrating migration/rollback ticket paths are excluded or inactive under OFF builds.
- [x] 2.4 Add baseline behavior check proving Bluetooth persistence remains functional when feature flag is OFF.

## 3. Documentation and Validation Evidence

- [x] 3.1 Update docs/bluetooth-l1-tests.md with section-6 AC1/AC2/AC3 test matrix and ON/OFF execution guidance.
- [x] 3.2 Document feasible upgrade/downgrade simulation expectations for L1 scope and note any limitations.
- [x] 3.3 Capture regression validation evidence references for section-6 closure under both feature-flag configurations.
