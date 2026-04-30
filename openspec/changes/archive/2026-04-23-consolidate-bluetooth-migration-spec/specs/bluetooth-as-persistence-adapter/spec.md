## MODIFIED Requirements

### Requirement: Adapter integration remains compile-time isolated
All adapter compilation units and call sites for migration support SHALL be excluded when BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION is disabled.

#### Scenario: Build with migration flag OFF excludes adapter path
- **WHEN** Bluetooth is built with BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION disabled
- **THEN** adapter code is not compiled or linked into migration and rollback call paths
