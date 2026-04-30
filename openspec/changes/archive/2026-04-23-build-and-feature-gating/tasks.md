## 1. Build Option And Define Wiring

- [x] 1.1 Add BLUETOOTH_ENABLE_AS_MIGRATION option in Bluetooth/CMakeLists.txt with default ON
- [x] 1.2 Export a single target compile definition derived from BLUETOOTH_ENABLE_AS_MIGRATION
- [x] 1.3 Verify platform build configuration can override BLUETOOTH_ENABLE_AS_MIGRATION

## 2. AC1 And AC2 Compile-Time Isolation

- [x] 2.1 Identify all CPESP-9452 AC1/AC2 code blocks and helper wiring in Bluetooth module
- [x] 2.2 Guard AC1 migration code paths under the unified compile-time define
- [x] 2.3 Guard AC2 rollback sync code paths under the same compile-time define
- [x] 2.4 Ensure adapter compilation units and integration points are included only when the gate is enabled

## 3. Baseline Behavior Protection

- [ ] 3.1 Confirm non-ticket Bluetooth persistence paths compile and run when BLUETOOTH_ENABLE_AS_MIGRATION is OFF
- [x] 3.2 Remove or refactor any stray ticket-level dependency from always-on code paths

## 4. Validation Matrix And Evidence

- [ ] 4.1 Build and run validation with BLUETOOTH_ENABLE_AS_MIGRATION ON and capture evidence
- [ ] 4.2 Build and run validation with BLUETOOTH_ENABLE_AS_MIGRATION OFF and capture evidence
- [x] 4.3 Add or update L1 coverage for ON/OFF compile-time gating expectations
- [ ] 4.4 Attach ON/OFF validation artifacts to CPESP-9452 and document deprecation-readiness criteria

## 5. Resolved Scope Alignment

- [x] 5.1 Keep option-forwarding changes scoped to Bluetooth/CMakeLists.txt only unless a new platform blocker is identified
- [x] 5.2 Do not add a dedicated CI OFF-matrix target in this change; track it separately only if future policy requires it
