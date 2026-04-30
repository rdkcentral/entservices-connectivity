# CPESP-9452 Section 6 Regression Validation Evidence

Change: tests-and-regression-coverage
Date: 2026-04-22
Owner: <fill>

## Objective
Capture raw L1 validation outputs proving AC1, AC2, and AC3 regression coverage under feature-flag ON and OFF configurations.

## Scenario Coverage Map
- AC1.1 Existing PersistentStore data bypasses AS import
- AC1.2 Missing PersistentStore with valid AS payload imports and persists
- AC1.3 Missing PersistentStore with missing AS source is graceful and later syncs on mutation
- AC1.4 Missing PersistentStore with malformed AS payload remains non-fatal
- AC2.1 Mutation mirrors to AS output when flag ON
- AC2.2 Mutation does not mirror to AS output when flag OFF
- AC3.1 Compile-gated ticket path inactive for OFF build
- AC3.2 Baseline Bluetooth persistence behavior remains functional for OFF build

## Raw Command Outputs
Paste terminal output exactly as executed.

### 1) Build and run L1 tests with flag ON
Command:
```bash
<fill build/test command for BLUETOOTH_ENABLE_AS_MIGRATION=ON>
```
Output:
```text
<paste raw output>
```

### 2) Build and run L1 tests with flag OFF
Command:
```bash
<fill build/test command for BLUETOOTH_ENABLE_AS_MIGRATION=OFF>
```
Output:
```text
<paste raw output>
```

### 3) Test name extraction for section-6 scenarios
Command:
```bash
rg -n "asMigration|rollbackSync|featureGateCompileCoverage" Tests/L1Tests/tests/test_Bluetooth.cpp
```
Output:
```text
<paste raw output>
```

## Evidence Checklist
- [ ] AC1 scenarios demonstrated in ON run output
- [ ] AC2 ON mirror scenario demonstrated
- [ ] AC2 OFF no-mirror scenario demonstrated
- [ ] AC3 OFF compile-gating and baseline behavior demonstrated
- [ ] Raw outputs pasted without modification
