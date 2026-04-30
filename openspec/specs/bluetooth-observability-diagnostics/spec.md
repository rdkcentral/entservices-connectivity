## ADDED Requirements

### Requirement: Migration lifecycle SHALL emit structured diagnostics
Migration flow SHALL emit structured logs for attempted, successful, and failed migration outcomes so AC1 diagnostics are traceable in validation runs.

#### Scenario: Migration attempted
- **WHEN** initialization detects missing PersistentStore metadata and enters migration decision path
- **THEN** a structured migration-attempt log is emitted

#### Scenario: Migration success
- **WHEN** AS migration data is imported and persisted successfully
- **THEN** a structured migration-success log is emitted

#### Scenario: Migration failure or fallback
- **WHEN** migration source is missing or malformed, or migration import fails
- **THEN** structured migration-failure-or-fallback diagnostics are emitted without crashing initialization

### Requirement: Rollback synchronization SHALL emit structured diagnostics
Rollback synchronization flow SHALL emit structured success and failure diagnostics whenever AS sync is attempted after persistence updates.

#### Scenario: Rollback sync success
- **WHEN** cache-to-store update succeeds and AS sync write succeeds
- **THEN** a structured rollback-sync-success log is emitted

#### Scenario: Rollback sync failure
- **WHEN** cache-to-store update succeeds but AS sync write fails
- **THEN** a structured rollback-sync-failure log is emitted and PersistentStore update remains authoritative

### Requirement: Telemetry markers SHALL map to section-7 lifecycle events
Telemetry emission points SHALL include migration_attempted, migration_success, migration_failure, rollback_sync_success, and rollback_sync_failure markers.

#### Scenario: Marker coverage validation
- **WHEN** section-7 validation evidence is captured
- **THEN** each required marker is observable or explicitly accounted for by scenario outcome

### Requirement: Diagnostics SHALL be scenario-traceable for evidence collection
Documentation and evidence artifacts SHALL define how logs and telemetry outputs map to section-7 checklist scenarios.

#### Scenario: Evidence traceability review
- **WHEN** reviewers inspect section-7 evidence artifacts
- **THEN** each observability scenario has a corresponding log and telemetry reference
