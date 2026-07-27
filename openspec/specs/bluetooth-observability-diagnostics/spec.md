## ADDED Requirements

### Requirement: Migration lifecycle SHALL emit structured diagnostics
Migration flow SHALL emit structured logs for attempted and completed migration outcomes — including the empty-payload case where the AS source is absent — and for failed migration outcomes, so AC1 diagnostics are traceable in validation runs. These markers are emitted by `performMigration()`, which is a client-triggered API call; initialization does not perform auto-migration and does not emit these markers.

#### Scenario: Migration attempted
- **WHEN** a client invokes `performMigration()`
- **THEN** a structured migration-attempt log is emitted at the start of the call

#### Scenario: Migration success
- **WHEN** `performMigration()` imports AS data and persists it to PersistentStore successfully
- **THEN** a structured migration-success log is emitted and the call returns `ERROR_NONE`

#### Scenario: Migration success (source not found — empty payload)
- **WHEN** `performMigration()` is called but the AS filesystem persistence source does not exist
- **THEN** the adapter logs that the file does not exist, the missing source is treated as an empty payload, migration completes with an empty device list persisted to PersistentStore and migrationVersion written, a structured migration-success log is emitted, and the call returns `ERROR_NONE`

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
Log emission points SHALL include migration_attempted, migration_success, migration_failure, rollback_sync_success, and rollback_sync_failure markers. The migration_* markers are owned by `performMigration()` and are only observable when that API is explicitly called; they are not emitted during plugin initialization. There is no distinct migration_skipped marker — a missing source produces an empty payload and the call completes under migration_success.

#### Scenario: Marker coverage validation
- **WHEN** section-7 validation evidence is captured
- **THEN** each required log marker is observable or explicitly accounted for by scenario outcome

### Requirement: Diagnostics SHALL be scenario-traceable for evidence collection
Documentation and evidence artifacts SHALL define how log outputs map to section-7 checklist scenarios.

#### Scenario: Evidence traceability review
- **WHEN** reviewers inspect section-7 evidence artifacts
- **THEN** each observability scenario has a corresponding log marker reference
