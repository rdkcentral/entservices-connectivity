## Context

Section 7 of CPESP-9452 requires clear runtime observability for migration (AC1) and rollback synchronization (AC2) behavior so support teams can verify outcomes and diagnose failures. Existing persistence logic has basic logging but does not yet provide a complete, scenario-aligned diagnostics contract with explicit telemetry markers for each required state.

## Goals / Non-Goals

**Goals:**
- Define structured log points for migration start, success, and failure paths.
- Define structured log points for rollback synchronization success and failure paths.
- Define telemetry markers for migration and rollback lifecycle events aligned with checklist acceptance criteria.
- Ensure section-7 validation evidence can map each scenario to deterministic log and telemetry output.

**Non-Goals:**
- Introduce new persistence behavior or change migration/rollback control flow semantics.
- Add external telemetry dependencies beyond existing project mechanisms.
- Redesign plugin APIs or schema mappings.

## Decisions

- Event taxonomy locking:
  - Decision: use explicit event names for migration and rollback lifecycle markers: migration_attempted, migration_success, migration_failure, rollback_sync_success, rollback_sync_failure.
  - Rationale: fixed naming prevents interpretation drift across code, tests, and operational dashboards.
  - Alternative considered: free-form log-only diagnostics. Rejected due to weak machine-readability and inconsistent triage.

- Structured diagnostic logging at decision points:
  - Decision: emit structured logs at migration entry, migration outcomes, rollback sync attempt outcomes, and non-fatal fallback paths.
  - Rationale: section-7 acceptance expects scenario-to-log mapping during validation runs.
  - Alternative considered: only failure logging. Rejected because success-path observability is needed to prove expected behavior.

- Non-invasive instrumentation strategy:
  - Decision: add diagnostics and telemetry only at existing control points in BluetoothDeviceManager.cpp without altering persistence decision order.
  - Rationale: preserves behavior while increasing visibility.
  - Alternative considered: refactor logic for instrumentation convenience. Rejected to minimize regression risk.

## Risks / Trade-offs

- [Risk] Increased log volume in frequent mutating paths. -> Mitigation: use concise structured messages focused on state transitions.
- [Risk] Telemetry marker emission could be skipped in edge branches. -> Mitigation: instrument all explicit success/failure branches in migration and rollback paths.
- [Risk] Validation environments may not expose telemetry transport uniformly. -> Mitigation: pair telemetry markers with corresponding logs and provide evidence template guidance.

## Migration Plan

1. Add structured migration logs and marker emissions in migration-triggered branches.
2. Add rollback sync success/failure logs and marker emissions in persistence update flow.
3. Add observability evidence template with raw command output placeholders.
4. Validate marker-to-scenario traceability through section-7 external runs.

## Open Questions

- Whether telemetry marker aggregation is consumed through a centralized pipeline in all target environments, or requires environment-specific collection instructions in evidence files.
