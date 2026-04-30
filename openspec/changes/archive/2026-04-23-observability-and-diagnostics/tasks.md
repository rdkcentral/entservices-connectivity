## 1. Migration Diagnostics Instrumentation

- [x] 1.1 Add structured log at migration entry when PersistentStore data is missing and migration path is considered.
- [x] 1.2 Add structured migration success log when AS data import and persistence complete.
- [x] 1.3 Add structured migration failure/fallback logs for missing AS source and malformed/invalid source data.
- [x] 1.4 Emit telemetry markers for migration_attempted, migration_success, and migration_failure in corresponding branches.

## 2. Rollback Sync Diagnostics Instrumentation

- [x] 2.1 Add structured rollback sync success log after successful AS-file synchronization.
- [x] 2.2 Add structured rollback sync failure log when AS write fails after PersistentStore success.
- [x] 2.3 Emit telemetry markers rollback_sync_success and rollback_sync_failure aligned to rollback outcomes.
- [x] 2.4 Preserve existing failure policy semantics (do not roll back PersistentStore update on AS sync failure).

## 3. Observability Validation Assets

- [x] 3.1 Add section-7 observability evidence template in the change folder with raw output placeholders.
- [x] 3.2 Define log and marker mapping from section-7 checklist scenarios to concrete validation commands.
- [x] 3.3 Verify scenario-to-diagnostic traceability can be demonstrated in external validation runs.
