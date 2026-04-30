## Context

CPESP-9452 introduces temporary migration and rollback behavior between Bluetooth plugin persistence and an AS JSON file. The repository already has a Bluetooth module with existing persistence and runtime mutation paths. The current need is to isolate all AC1 and AC2 ticket code under one compile-time switch so the project can ship with migration support enabled by default, test both modes, and remove ticket logic later without refactoring unrelated behavior.

Constraints:
- Baseline Bluetooth behavior must remain unchanged when the feature is OFF.
- AC1 and AC2 ticket code must be cleanly excluded at compile time when OFF.
- Existing module build patterns in Bluetooth/CMakeLists.txt must be preserved.

Stakeholders:
- Bluetooth plugin maintainers
- Platform integrators controlling build options
- Validation teams running flag ON/OFF test matrices

## Goals / Non-Goals

**Goals:**
- Provide one compile-time option, BLUETOOTH_ENABLE_AS_MIGRATION, default ON.
- Ensure the option exports a single preprocessor define consumed by all AC1/AC2 ticket code.
- Keep gating centralized to avoid scattered conditional logic.
- Define verifiable build outcomes for ON and OFF modes.

**Non-Goals:**
- Implementing full AC1 migration data logic in this change.
- Implementing full AC2 rollback synchronization logic in this change.
- Changing external Thunder API contracts.

## Decisions

1. Use one unified compile-time option for both AC1 and AC2.
- Decision: Introduce BLUETOOTH_ENABLE_AS_MIGRATION in Bluetooth/CMakeLists.txt with default ON.
- Rationale: A single switch satisfies AC3, simplifies platform controls, and makes removal criteria explicit.
- Alternatives considered:
  - Separate AC1 and AC2 flags: rejected due to combinatorial testing and higher maintenance cost.
  - Runtime-only config toggle: rejected because AC3 requires compile-time isolation/removability.

2. Export one define and gate only ticket-scoped code.
- Decision: Convert the CMake option into one define and wrap only CPESP-9452 code blocks, helper wiring, and adapter compilation units.
- Rationale: Limits preprocessor noise and protects baseline plugin behavior.
- Alternatives considered:
  - Guard entire Bluetooth manager implementation: rejected because it risks behavior divergence and complicates review.

3. Keep optional runtime companion config as documentation-only unless platform requests it.
- Decision: Do not require Bluetooth.config changes by default; document optional precedence if added later.
- Rationale: Avoids introducing unnecessary runtime control surface for a compile-time requirement.
- Alternatives considered:
  - Mandatory runtime companion knob: rejected as over-scoping this phase.

## Risks / Trade-offs

- [Risk] Gate coverage misses a mutation path and compiles partial ticket logic when OFF.
  - Mitigation: Explicitly enumerate mutation paths in checklist/tests and enforce OFF-build validation.

- [Risk] Excessive preprocessor conditionals reduce readability.
  - Mitigation: Centralize gated helper interfaces and keep conditionals at module boundaries.

- [Risk] Integrator confusion about ON/OFF expectations.
  - Mitigation: Document acceptance exits and flag semantics in implementation checklist and test docs.

## Migration Plan

1. Add CMake option and define with default ON.
2. Build and validate ON path with current migration/rollback code enabled.
3. Build and validate OFF path to confirm AC1/AC2 ticket code is excluded and baseline behavior remains.
4. Attach ON/OFF evidence to CPESP-9452.

Rollback strategy:
- If unexpected regressions occur, keep default ON while fixing guard coverage; do not merge partial OFF behavior without validation evidence.

## Resolved Questions

- Question: Whether any platform build wrapper requires explicit option forwarding beyond Bluetooth/CMakeLists.txt.
  - Answer: No.
  - Decision impact: No additional platform-wrapper forwarding work is required in this change.

- Question: Whether CI matrix should add a dedicated OFF target for long-term regression protection.
  - Answer: No.
  - Decision impact: CI matrix changes are out of scope for this change; ON/OFF validation evidence remains required for CPESP-9452.
