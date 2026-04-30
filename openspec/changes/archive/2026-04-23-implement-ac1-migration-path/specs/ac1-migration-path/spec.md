## ADDED Requirements

### Requirement: Initialization SHALL prioritize Bluetooth PersistentStore before AS migration
The Bluetooth initialization flow SHALL check Bluetooth PersistentStore device info first and SHALL continue normal startup without AS import when data exists.

#### Scenario: PersistentStore contains device info
- **WHEN** initialization reads device info successfully from Bluetooth PersistentStore
- **THEN** AC1 AS-file import path is not executed

### Requirement: Initialization SHALL import from AS only when store data is absent
When Bluetooth PersistentStore device info is absent, the system SHALL attempt to read AS file data, map it to plugin cache, and persist it into Bluetooth PersistentStore.

#### Scenario: Store absent and AS payload valid
- **WHEN** PersistentStore returns not-exist and AS payload is valid
- **THEN** initialization imports AS data into cache and writes migrated data to Bluetooth PersistentStore

### Requirement: Initialization SHALL remain non-fatal on AS migration read or parse failure
The AC1 migration path SHALL not fail plugin initialization when AS file is missing, unreadable, or malformed.

#### Scenario: Store absent and AS payload invalid
- **WHEN** PersistentStore returns not-exist and AS read or parse fails
- **THEN** initialization logs migration failure and continues with existing device reconciliation behavior

### Requirement: AC1 migration path SHALL be compile-time isolated
All AC1 migration declarations and code paths SHALL be excluded from OFF-mode builds when BLUETOOTH_ENABLE_AS_MIGRATION is disabled.

#### Scenario: Build with migration flag OFF
- **WHEN** Bluetooth is compiled with BLUETOOTH_ENABLE_AS_MIGRATION disabled
- **THEN** AC1 migration orchestration code is not compiled into runtime behavior
