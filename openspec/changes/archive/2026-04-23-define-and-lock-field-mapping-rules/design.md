## Context

Section 5 of CPESP-9452 requires a single authoritative mapping contract across specs and docs for AS persistence fields and plugin model representations. Current behavior is spread across adapter code, checklist notes, and migration/rollback narratives, making it easy for naming or type assumptions to diverge over time.

## Goals / Non-Goals

**Goals:**
- Define and lock a canonical mapping table for AS fields to plugin model fields and conversion semantics.
- Capture resolved policy decisions for deviceAddr identity, timestamp representation, and lastVolumeSetting authority.
- Keep spec and docs language consistent so tests and validation evidence use the same field definitions.

**Non-Goals:**
- Implement new runtime behavior beyond documenting and locking mapping rules.
- Change AC1/AC2 control-flow logic or compile-time gating behavior.
- Introduce new schema fields outside already accepted CPESP-9452 scope.

## Decisions

- Canonical mapping source alignment:
  - Decision: maintain one explicit mapping table in spec and mirror it in docs with identical naming and conversion notes.
  - Rationale: prevents drift between implementation expectations and operational documentation.
  - Alternative considered: prose-only mapping description. Rejected due to ambiguity and review overhead.

- Identity and authority rules:
  - Decision: deviceAddr maps to Bluetooth MAC identity; lastConnectionTimeUTC follows schema representation consistently across local file and PersistentStore; lastVolumeSetting remains BTMgr-authoritative.
  - Rationale: these are resolved CPESP-9452 policies and should be normative constraints in artifacts.
  - Alternative considered: allow implementation-specific interpretation. Rejected because it undermines interoperability and testability.

- Failure policy documentation coupling:
  - Decision: adapter failure-handling policy is documented in design notes alongside mapping decisions so field semantics and failure semantics are reviewed together.
  - Rationale: rollback/migration interpretation depends on both mapping and failure behavior.
  - Alternative considered: separate policy references across files without linkage. Rejected due to inconsistency risk.

## Risks / Trade-offs

- [Risk] Existing docs may use different field labels or implied types. -> Mitigation: use one table and synchronize wording verbatim across target files.
- [Risk] Future code updates can drift from documented mapping. -> Mitigation: treat table as normative reference for AC1/AC2 reviews and L1 expectations.
- [Risk] Schema evolution may require later updates. -> Mitigation: record current decisions explicitly and update via future scoped change when schema changes.

## Migration Plan

1. Add explicit mapping table in spec artifact for AS and plugin model fields.
2. Mirror the same table and conversion notes in Bluetooth plugin docs.
3. Add adapter decision and failure-policy notes in design-notes artifact.
4. Cross-check all three files for name/type consistency before closing change.

## Open Questions

- None. Section 5 decisions in CPESP-9452 are resolved and this change encodes them as locked documentation rules.
