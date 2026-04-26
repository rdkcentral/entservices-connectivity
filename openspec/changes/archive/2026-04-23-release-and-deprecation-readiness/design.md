## Context

CPESP-9452 introduced a compile-time-gated migration and rollback path that bridges AS-file persistence and Bluetooth PersistentStore. Sections 1 through 8 established functional behavior, diagnostics, and safeguards, but Section 9 requires explicit release policy and deprecation criteria so this temporary path does not become permanent technical debt.

Current state has implementation and tests for AC1/AC2/AC3, plus logs-oriented diagnostics. What is missing is a normative readiness contract that states when the gated path may be retained, and when it must be removed.

## Goals / Non-Goals

**Goals:**
- Define authoritative policy statements that must be captured in bluetooth design/spec artifacts.
- Define objective, auditable deprecation trigger criteria for removing BLUETOOTH_ENABLE_AS_MIGRATION ticket path code.
- Ensure readiness criteria are tied to validation evidence and release governance.

**Non-Goals:**
- No runtime functional behavior change in Bluetooth plugin logic.
- No additional migration/rollback implementation code in this change.
- No CI/CD workflow restructuring beyond documenting criteria and evidence expectations.

## Decisions

1. Policy decisions are codified as release constraints, not implementation comments.
- Rationale: policy must be discoverable and reviewable in spec/design artifacts.
- Alternative considered: keeping policy in PR notes only.
- Why not: ephemeral and not enforceable across releases.

2. Deprecation uses gated, evidence-driven triggers.
- Rationale: removing AC1/AC2 flag path without objective evidence risks regressions in downgrade or mixed-fleet scenarios.
- Alternative considered: time-based deprecation after one release.
- Why not: schedule-only criteria do not prove operational safety.

3. Diagnostics policy remains aligned with active implementation track.
- Rationale: section-7 implementation moved to structured logs-only diagnostics; section-9 readiness should not reintroduce conflicting mandatory telemetry requirements.
- Alternative considered: re-require telemetry markers for deprecation readiness.
- Why not: conflicts with accepted logs-only direction and would introduce inconsistent governance.

## Risks / Trade-offs

- [Risk] Criteria are too strict, delaying cleanup of legacy path -> Mitigation: include a minimum evidence set and permit explicit waiver review.
- [Risk] Criteria are too weak, causing premature deprecation -> Mitigation: require both functional regression evidence and field-observability confirmation.
- [Risk] Policy drift between checklist, docs, and implementation -> Mitigation: define one section-9 capability spec as source for readiness gates.

## Migration Plan

1. Update section-9 design/spec artifacts with resolved policies and deprecation criteria.
2. Attach evidence references for AC1/AC2/AC3 behavior and non-functional safeguards.
3. During release review, evaluate criteria and decide:
- retain BLUETOOTH_ENABLE_AS_MIGRATION path for another cycle, or
- execute removal change after criteria are fully met.
4. For rollback, if criteria are not met post-cut, continue building with flag path enabled and defer removal change.

## Open Questions

- What exact release count threshold is required before deprecation can be approved (for example, one or two stable release cycles)?
- Is field telemetry-free observability sufficient for governance sign-off when logs are the approved diagnostics channel?
