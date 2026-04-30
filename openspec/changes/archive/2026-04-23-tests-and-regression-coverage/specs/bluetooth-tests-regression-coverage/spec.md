## ADDED Requirements

### Requirement: AC1 migration regression scenarios SHALL be covered in L1 tests
L1 tests SHALL verify AC1 migration behavior for all required fallback and import scenarios: PersistentStore-present bypass, missing PersistentStore with valid AS import and persist, missing PersistentStore with missing AS source fallback, and missing PersistentStore with malformed AS source fallback.

#### Scenario: PersistentStore-present bypass
- **WHEN** initialization runs with existing PersistentStore Bluetooth/deviceInfo data
- **THEN** no AS import path is executed
- **THEN** initialization proceeds with existing persistent data semantics

#### Scenario: Valid AS import path
- **WHEN** PersistentStore Bluetooth/deviceInfo is missing and AS file payload is valid
- **THEN** migration imports AS data into cache and persists normalized values to PersistentStore

#### Scenario: Missing AS source fallback
- **WHEN** PersistentStore Bluetooth/deviceInfo is missing and AS source file is unavailable
- **THEN** initialization remains non-fatal and subsequent persistence updates write expected outputs

#### Scenario: Malformed AS source fallback
- **WHEN** PersistentStore Bluetooth/deviceInfo is missing and AS source payload is malformed
- **THEN** initialization remains non-fatal and fallback behavior is validated without crash

### Requirement: AC2 rollback synchronization SHALL be validated for feature-flag ON and OFF
L1 tests SHALL verify that mutating persistence paths mirror to AS output when BLUETOOTH_ENABLE_AS_MIGRATION is ON and do not mirror when it is OFF.

#### Scenario: Mirror enabled with flag ON
- **WHEN** a mutating path updates deviceInfo with migration flag ON
- **THEN** AS sync behavior is triggered and validated

#### Scenario: Mirror disabled with flag OFF
- **WHEN** the same mutating path executes with migration flag OFF
- **THEN** no AS sync mirror behavior is triggered and baseline persistence behavior remains functional

### Requirement: AC3 compile-time isolation coverage SHALL be enforced
L1 coverage SHALL include checks that migration/rollback ticket paths are excluded or inactive when BLUETOOTH_ENABLE_AS_MIGRATION is OFF, while baseline Bluetooth persistence behavior continues to work.

#### Scenario: Compile-gated helper exclusion
- **WHEN** tests are built and run with migration flag OFF
- **THEN** migration and rollback ticket-specific helpers are not required for baseline functional behavior

#### Scenario: Baseline behavior with flag OFF
- **WHEN** persistence operations run under flag OFF build
- **THEN** expected non-migration Bluetooth persistence semantics remain intact

### Requirement: Test documentation SHALL reflect section-6 matrix and execution guidance
Documentation SHALL be updated to include AC1/AC2/AC3 regression matrix entries and feature-flag ON/OFF execution expectations, including feasible upgrade and downgrade simulation coverage in L1 scope.

#### Scenario: Documentation traceability
- **WHEN** reviewers inspect Bluetooth L1 test documentation
- **THEN** each section-6 required scenario maps to explicit test and execution expectations
