## MODIFIED Requirements

### Requirement: Unified compile-time migration gate
The Bluetooth module SHALL provide a compile-time option named BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION that is enabled by default and controls migration and rollback synchronization code paths.

#### Scenario: Default configuration includes migration paths
- **WHEN** Bluetooth is built without overriding BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
- **THEN** migration import and rollback synchronization paths are compiled into the module

#### Scenario: Disabled configuration excludes migration paths
- **WHEN** Bluetooth is built with BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION disabled
- **THEN** migration and rollback synchronization ticket paths are excluded from compilation

### Requirement: Single define propagation for gate enforcement
The build system MUST export one preprocessor define derived from BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION and migration-related code paths MUST rely on that define for compile-time inclusion or exclusion.

#### Scenario: Define present when gate is enabled
- **WHEN** BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is enabled
- **THEN** the Bluetooth target receives the corresponding preprocessor define for migration-path compilation

#### Scenario: Define absent or disabled when gate is disabled
- **WHEN** BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is disabled
- **THEN** migration-specific helper wiring and adapter integration are not compiled

### Requirement: Baseline behavior preserved when gate is disabled
When BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is disabled, Bluetooth persistence behavior unrelated to migration support MUST remain operational and behaviorally consistent with baseline behavior.

#### Scenario: Baseline persistence remains functional
- **WHEN** BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is disabled and runtime mutations occur
- **THEN** existing non-migration Bluetooth persistence flows continue to operate without requiring migration code paths
