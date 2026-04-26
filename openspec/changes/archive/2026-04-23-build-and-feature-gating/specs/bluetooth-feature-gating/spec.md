## ADDED Requirements

### Requirement: Unified compile-time migration gate
The Bluetooth module SHALL provide a compile-time option named BLUETOOTH_ENABLE_AS_MIGRATION that is enabled by default and controls all CPESP-9452 AC1 and AC2 ticket code paths.

#### Scenario: Default configuration includes ticket paths
- **WHEN** Bluetooth is built without overriding BLUETOOTH_ENABLE_AS_MIGRATION
- **THEN** AC1 migration and AC2 rollback ticket code paths are compiled into the module

#### Scenario: Disabled configuration excludes ticket paths
- **WHEN** Bluetooth is built with BLUETOOTH_ENABLE_AS_MIGRATION disabled
- **THEN** AC1 and AC2 ticket code introduced by CPESP-9452 are excluded from compilation

### Requirement: Single define propagation for gate enforcement
The build system MUST export one preprocessor define derived from BLUETOOTH_ENABLE_AS_MIGRATION and all CPESP-9452 AC1 and AC2 logic MUST rely on that define for compile-time inclusion or exclusion.

#### Scenario: Define present when gate is enabled
- **WHEN** BLUETOOTH_ENABLE_AS_MIGRATION is enabled
- **THEN** the Bluetooth target receives the corresponding preprocessor define for AC1 and AC2 code compilation

#### Scenario: Define absent or disabled when gate is disabled
- **WHEN** BLUETOOTH_ENABLE_AS_MIGRATION is disabled
- **THEN** CPESP-9452-specific AC1 and AC2 helper wiring and adapter integration are not compiled

### Requirement: Baseline behavior preserved when gate is disabled
When BLUETOOTH_ENABLE_AS_MIGRATION is disabled, Bluetooth persistence behavior unrelated to CPESP-9452 MUST remain operational and behaviorally consistent with pre-ticket baseline.

#### Scenario: Baseline persistence remains functional
- **WHEN** BLUETOOTH_ENABLE_AS_MIGRATION is disabled and runtime mutations occur
- **THEN** existing non-ticket Bluetooth persistence flows continue to operate without requiring AC1/AC2 code paths
