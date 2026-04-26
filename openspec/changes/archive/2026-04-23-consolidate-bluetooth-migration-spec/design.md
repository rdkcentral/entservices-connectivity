## Context

Bluetooth migration behavior is currently documented across multiple capabilities that were introduced incrementally (AC1, AC2, feature-gating, adapter integration, naming cleanup, test coverage, and release readiness). The implementation has converged to a current-state model using `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`, but several specs still reference superseded naming (`BLUETOOTH_ENABLE_AS_MIGRATION`) or duplicate migration contract statements.

This change is documentation-focused: establish a single canonical migration capability and reduce ambiguity in legacy capability specs by de-duplicating and aligning terminology.

## Goals / Non-Goals

**Goals:**
- Introduce one canonical capability (`bluetooth-migration`) for current migration, rollback sync, and compile-time gating behavior.
- Align migration-related terminology across existing capabilities to the current code identifiers.
- Reduce duplicated or stale requirement statements in older split capabilities.
- Keep requirement traceability for audit/review by preserving clear migration pointers from legacy specs to the new canonical capability.

**Non-Goals:**
- No runtime code changes in Bluetooth implementation.
- No behavioral redesign of migration import or rollback synchronization.
- No changes to observability/logging-only capabilities beyond terminology alignment where explicitly required.
- No deletion of capability directories from `openspec/specs/`; this change uses spec deltas to supersede/align requirements.

## Decisions

### Decision 1: Introduce `bluetooth-migration` as canonical migration contract
- Rationale: A single normative source eliminates requirement drift and simplifies implementation/test traceability.
- Alternative considered: Keep split AC1/AC2/feature-gating specs as primary sources and only rename terms.
- Why not chosen: It retains duplication and continues to spread migration semantics across multiple files.

### Decision 2: Deprecate split AC1/AC2 requirement blocks via explicit REMOVED deltas with migration guidance
- Rationale: AC1/AC2 split requirements are now conceptual slices of one integrated migration flow and should be marked superseded, not silently ignored.
- Alternative considered: Leave AC1/AC2 unchanged and add informational notes elsewhere.
- Why not chosen: Leaves conflicting normative requirements active and increases review ambiguity.

### Decision 3: Use MODIFIED deltas for capabilities that remain active but require terminology alignment
- Rationale: Capabilities such as feature-gating, tests, and release-readiness remain relevant but must reflect current compile-time flag naming.
- Alternative considered: Remove these capabilities entirely.
- Why not chosen: They still provide distinct governance/testing concerns and should remain independently traceable.

### Decision 4: Preserve adapter-specific requirements while removing migration-contract duplication
- Rationale: Adapter read/write behavior and parse/write guarantees are adapter-scoped and should remain there, while migration orchestration semantics move to the canonical capability.
- Alternative considered: Move all adapter requirements into `bluetooth-migration`.
- Why not chosen: Blurs module boundaries and weakens adapter-specific testability.

## Risks / Trade-offs

- [Risk] Reviewers may interpret legacy capabilities as fully obsolete instead of partially superseded.
  - Mitigation: Use explicit `Reason` and `Migration` text in REMOVED deltas and scoped MODIFIED deltas.

- [Risk] Over-consolidation may hide AC1/AC2 historical context needed for backward audit trails.
  - Mitigation: Keep legacy capability files and reference the new canonical capability rather than deleting history.

- [Risk] Terminology alignment could be incomplete if downstream docs still use old names.
  - Mitigation: Include alignment tasks to search active specs/docs for stale migration flag naming.

## Migration Plan

1. Add new capability delta for `bluetooth-migration` with full current-state requirements and scenarios.
2. Apply deltas to listed modified capabilities:
   - supersede split migration requirements where appropriate,
   - rename stale compile-time flag references,
   - preserve capability-specific concerns.
3. Validate that every capability declared in proposal has a corresponding delta file under this change.
4. During apply, sync deltas to main specs and verify no active migration capability still uses superseded compile-time flag naming.

## Open Questions

- Should a future cleanup change archive fully superseded legacy capabilities (`ac1-migration-path`, `ac2-rollback-sync-path`) after downstream consumers migrate to `bluetooth-migration`?
- Should Section 9 release-readiness artifacts be updated in parallel to reference consolidated capability IDs for checklist automation?
