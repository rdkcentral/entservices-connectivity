## ADDED Requirements

### Requirement: AC2 rollback sync SHALL occur after successful PersistentStore writes
When Bluetooth device info is persisted successfully, the system SHALL trigger AS-file synchronization through the adapter when BLUETOOTH_ENABLE_AS_MIGRATION is enabled.

#### Scenario: Successful persistence triggers AS sync
- **WHEN** updateStorageFromCache completes with PersistentStore success
- **THEN** AS synchronization is invoked for rollback path consistency

### Requirement: AC2 sync SHALL cover all mutating persistence paths
All mutating operations that write deviceInfo data SHALL execute through the shared persistence path so AC2 sync behavior is applied consistently.

#### Scenario: Mutating API path writes and triggers sync
- **WHEN** a mutating operation updates cache and persists deviceInfo
- **THEN** the operation reaches shared persistence flow and executes AC2 sync hook on success

### Requirement: AS sync failure SHALL not roll back PersistentStore success
If rollback sync write to AS fails, the system SHALL log and emit diagnostics while preserving the successful PersistentStore update.

#### Scenario: AS write failure after persistence success
- **WHEN** PersistentStore write succeeds and AS sync write fails
- **THEN** system records failure diagnostics and does not revert PersistentStore data

### Requirement: AC2 rollback path SHALL be compile-time isolated
AC2 rollback sync logic and integration points SHALL be excluded from OFF-mode builds when BLUETOOTH_ENABLE_AS_MIGRATION is disabled.

#### Scenario: Build with migration flag OFF excludes AC2 path
- **WHEN** Bluetooth module is built with BLUETOOTH_ENABLE_AS_MIGRATION disabled
- **THEN** rollback sync code paths are not compiled into runtime behavior
