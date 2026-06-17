## ADDED Requirements

### Requirement: Migration lifecycle SHALL emit structured diagnostics
Migration flow SHALL emit structured logs for attempted, successful, and failed migration outcomes so AC1 diagnostics are traceable in validation runs. These markers are emitted by `performMigration()`, which is a client-triggered API call; initialization does not perform auto-migration and does not emit these markers.

#### Scenario: Migration attempted
- **WHEN** a client invokes `performMigration()`
- **THEN** a structured migration-attempt log is emitted at the start of the call

#### Scenario: Migration success
- **WHEN** `performMigration()` imports AS data and persists it to PersistentStore successfully
- **THEN** a structured migration-success log is emitted and the call returns `ERROR_NONE`

#### Scenario: Migration skipped (source not found)
- **WHEN** `performMigration()` is called but the AS filesystem persistence source does not exist
- **THEN** a structured migration-skipped log is emitted and the call returns without importing migration data

#### Scenario: Migration failure or fallback
- **WHEN** `performMigration()` is called and the migration source exists but is malformed or unreadable, or the import or persistence step fails for another reason
- **THEN** structured migration-failure-or-fallback diagnostics are emitted and the call returns a non-`ERROR_NONE` result code

### Requirement: Rollback synchronization SHALL emit structured diagnostics
Rollback synchronization flow SHALL emit structured success and failure diagnostics whenever AS sync is attempted after persistence updates.

#### Scenario: Rollback sync success
- **WHEN** cache-to-store update succeeds and AS sync write succeeds
- **THEN** a structured rollback-sync-success log is emitted

#### Scenario: Rollback sync failure
- **WHEN** cache-to-store update succeeds but AS sync write fails
- **THEN** a structured rollback-sync-failure log is emitted and PersistentStore update remains authoritative

### Requirement: Log markers SHALL map to section-7 lifecycle events
Log emission points SHALL include migration_attempted, migration_success, migration_skipped, migration_failure, rollback_sync_success, and rollback_sync_failure markers. The migration_* markers are owned by `performMigration()` and are only observable when that API is explicitly called; they are not emitted during plugin initialization.

#### Scenario: Marker coverage validation
- **WHEN** section-7 validation evidence is captured
- **THEN** each required log marker is observable or explicitly accounted for by scenario outcome

### Requirement: Diagnostics SHALL be scenario-traceable for evidence collection
Documentation and evidence artifacts SHALL define how log outputs map to section-7 checklist scenarios.

#### Scenario: Evidence traceability review
- **WHEN** reviewers inspect section-7 evidence artifacts
- **THEN** each observability scenario has a corresponding log marker reference
