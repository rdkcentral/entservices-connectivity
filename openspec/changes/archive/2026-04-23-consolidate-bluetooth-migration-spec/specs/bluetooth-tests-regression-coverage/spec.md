## MODIFIED Requirements

### Requirement: AC2 rollback synchronization SHALL be validated for feature-flag ON and OFF
L1 tests SHALL verify that mutating persistence paths mirror to AS output when BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is ON and do not mirror when it is OFF.

#### Scenario: Mirror enabled with flag ON
- **WHEN** a mutating path updates deviceInfo with migration flag ON
- **THEN** AS sync behavior is triggered and validated

#### Scenario: Mirror disabled with flag OFF
- **WHEN** the same mutating path executes with migration flag OFF
- **THEN** no AS sync mirror behavior is triggered and baseline persistence behavior remains functional

### Requirement: AC3 compile-time isolation coverage SHALL be enforced
L1 coverage SHALL include checks that migration/rollback ticket paths are excluded or inactive when BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is OFF, while baseline Bluetooth persistence behavior continues to work.

#### Scenario: Compile-gated helper exclusion
- **WHEN** tests are built and run with migration flag OFF
- **THEN** migration and rollback ticket-specific helpers are not required for baseline functional behavior

#### Scenario: Baseline behavior with flag OFF
- **WHEN** persistence operations run under flag OFF build
- **THEN** expected non-migration Bluetooth persistence semantics remain intact
