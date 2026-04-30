## Context

CPESP-9452 implementation already added migration and rollback behavior behind compile-time gating, but section 6 requires durable regression coverage that validates AC1, AC2, and AC3 behavior under both feature-flag configurations. Existing tests cover portions of behavior but do not yet provide complete scenario traceability for store-present bypass, malformed input fallback, and ON/OFF compile-gated mutation mirroring.

## Goals / Non-Goals

**Goals:**
- Define a deterministic L1 test matrix for AC1 migration, AC2 rollback sync, and AC3 compile-time isolation.
- Ensure tests can be executed with BLUETOOTH_ENABLE_AS_MIGRATION both ON and OFF without semantic ambiguity.
- Capture test documentation updates so expected scenario coverage remains discoverable and reviewable.
- Add feasible upgrade and downgrade simulation expectations in L1 scope.

**Non-Goals:**
- Introduce runtime behavior changes in Bluetooth plugin logic unrelated to testability.
- Add new production interfaces or persistence schema changes.
- Replace higher-level integration validation outside L1 scope.

## Decisions

- Dual-configuration validation strategy:
  - Decision: tests will explicitly validate behavior under feature flag ON and OFF, including mirror-write enabled/disabled assertions.
  - Rationale: AC3 acceptance requires compile-time isolation confidence, not only runtime checks.
  - Alternative considered: only run ON-path tests. Rejected because it cannot detect OFF-path regressions.

- Scenario-focused AC1 coverage:
  - Decision: add dedicated tests for PersistentStore-present bypass, valid AS import, missing AS source fallback, and malformed AS fallback.
  - Rationale: these scenarios directly match checklist acceptance language and reduce interpretation drift.
  - Alternative considered: broad integration-only test. Rejected because failure causes would be harder to localize.

- Documentation as test contract extension:
  - Decision: update Bluetooth L1 test documentation alongside code to preserve expected matrix and flag-specific behavior.
  - Rationale: test intent must be reviewable without reverse-engineering all fixtures.
  - Alternative considered: code-only changes. Rejected due to traceability gaps.

## Risks / Trade-offs

- [Risk] Environment-dependent fixtures could create flaky migration-path assertions. -> Mitigation: prefer deterministic fixture setup and explicit mock/store state control.
- [Risk] Compile-flag OFF validation may be skipped in some local environments. -> Mitigation: define ON/OFF command expectations in docs and evidence templates.
- [Risk] Upgrade/downgrade simulation may exceed pure L1 boundaries. -> Mitigation: record feasible expectations and document any limitations explicitly.

## Migration Plan

1. Extend L1 Bluetooth tests with AC1 scenario matrix and AC2 mirror behavior under ON/OFF configurations.
2. Add AC3 compile-time coverage checks proving gated path exclusion when flag is OFF.
3. Update Bluetooth L1 test documentation to include new scenario matrix and execution guidance.
4. Capture validation evidence for both feature-flag configurations.

## Open Questions

- Whether a single L1 run can reliably emulate full upgrade/downgrade lifecycle, or if this should be documented as a partial-scope expectation with external validation support.
