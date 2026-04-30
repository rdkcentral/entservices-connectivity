## Why

CPESP-9452 section 6 requires explicit regression validation for AC1 migration, AC2 rollback sync, and AC3 compile-time isolation behavior. Current coverage does not fully lock these expectations, leaving migration fallback and feature-flag ON/OFF behavior vulnerable to regressions.

## What Changes

- Add focused L1 test coverage for AC1 migration paths: store-present bypass, valid AS import path, missing AS source fallback, and malformed AS payload fallback.
- Add AC2 rollback sync tests for mutating persistence paths under feature flag ON and OFF configurations.
- Add AC3 coverage checks to ensure gated helpers and ticket-specific behavior are excluded when compile-time flag is OFF while baseline persistence behavior remains functional.
- Document section-6 test matrix updates in Bluetooth L1 test documentation.
- Define upgrade/downgrade simulation expectations where feasible in L1 scope.

## Capabilities

### New Capabilities
- `bluetooth-tests-regression-coverage`: Defines required L1 regression coverage and documentation updates for CPESP-9452 AC1, AC2, and AC3 behavior.

### Modified Capabilities
- None.

## Impact

- Affected code and tests:
  - `Tests/L1Tests/tests/test_Bluetooth.cpp`
  - `docs/bluetooth-l1-tests.md`
  - build-time test executions for feature-flag ON and OFF variants
- Behavioral impact:
  - no runtime API changes; this is verification and regression hardening
- Quality impact:
  - improved detection of migration/rollback/feature-gating regressions before integration
