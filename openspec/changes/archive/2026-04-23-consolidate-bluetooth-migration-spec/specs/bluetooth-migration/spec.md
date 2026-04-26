## ADDED Requirements

### Requirement: Migration initialization SHALL follow store-first import semantics
Bluetooth initialization SHALL read PersistentStore device info first and SHALL only execute migration import from the filesystem source when PersistentStore device info is absent.

#### Scenario: PersistentStore data present
- **WHEN** initialization finds valid Bluetooth/deviceInfo data in PersistentStore
- **THEN** filesystem migration import is skipped
- **THEN** initialization continues with existing persistent values

#### Scenario: PersistentStore data absent with valid migration source
- **WHEN** initialization finds no Bluetooth/deviceInfo data in PersistentStore and source payload is valid
- **THEN** migration data is imported into cache and persisted to PersistentStore

#### Scenario: PersistentStore data absent with invalid or missing migration source
- **WHEN** initialization finds no Bluetooth/deviceInfo data in PersistentStore and source payload is missing, unreadable, or malformed
- **THEN** initialization remains non-fatal and continues with baseline reconciliation flow

### Requirement: Rollback synchronization SHALL run after successful persistence writes
Mutating persistence paths SHALL invoke AS-file synchronization only after successful PersistentStore writes when migration support is enabled.

#### Scenario: Successful persistence triggers rollback synchronization
- **WHEN** a mutating operation persists deviceInfo successfully
- **THEN** rollback synchronization to the filesystem source is invoked in the success path

#### Scenario: Filesystem sync failure after persistence success
- **WHEN** PersistentStore write succeeds and filesystem sync fails
- **THEN** failure diagnostics are emitted
- **THEN** PersistentStore success remains authoritative and is not rolled back

### Requirement: Compile-time migration gating SHALL use canonical flag naming
Migration import and rollback synchronization code paths SHALL be controlled by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` at compile time.

#### Scenario: Migration gating enabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` enabled
- **THEN** migration and rollback synchronization paths are compiled and available

#### Scenario: Migration gating disabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` disabled
- **THEN** migration and rollback synchronization ticket paths are excluded from compilation

### Requirement: Baseline persistence behavior SHALL remain functional with migration disabled
Disabling migration support SHALL NOT break non-migration Bluetooth persistence behavior.

#### Scenario: Baseline behavior with migration disabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` disabled and runtime mutations occur
- **THEN** baseline Bluetooth persistence paths continue to function without migration dependencies
