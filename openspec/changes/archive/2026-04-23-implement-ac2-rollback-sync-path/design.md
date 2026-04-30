## Context

CPESP-9452 AC2 requires rollback synchronization from Bluetooth PersistentStore model updates back to the AS file after successful persistence writes. This behavior must be compile-time gated, must cover all mutating write paths, and must explicitly avoid transactional rollback when AS write fails. AC2 must integrate cleanly with existing updateStorageFromCache flow and adapter abstraction.

## Goals / Non-Goals

**Goals:**
- Trigger AS sync only after successful updateStorageFromCache writes.
- Ensure all mutating flows that persist deviceInfo invoke the same sync hook path.
- Preserve non-rollback failure policy for AS-write failures while maintaining diagnostics.
- Keep AC2 logic excluded when BLUETOOTH_ENABLE_AS_MIGRATION is disabled.

**Non-Goals:**
- Change AC1 migration trigger conditions.
- Redesign adapter schema mapping rules outside already resolved policy.
- Introduce runtime feature switches for AC2 path control.

## Decisions

- Single post-persist sync hook:
  - Decision: invoke updateAsFileFromCache from updateStorageFromCache on successful PersistentStore SetValue.
  - Rationale: centralizes AC2 behavior so every path that writes deviceInfo inherits sync semantics.
  - Alternative considered: call sync individually from each mutating method. Rejected due to duplication and drift risk.

- Failure handling without rollback:
  - Decision: AS sync failure logs and telemetry markers are emitted, but PersistentStore success remains authoritative and is not reverted.
  - Rationale: aligns with CPESP-9452 policy and avoids introducing multi-store transaction complexity.
  - Alternative considered: rollback PersistentStore on AS failure. Rejected as high-risk and out of scope.

- Compile-time isolation:
  - Decision: all AC2 sync calls and adapter wiring remain under BLUETOOTH_ENABLE_AS_MIGRATION.
  - Rationale: ensures OFF-mode exclusion and policy-compliant ticket isolation.
  - Alternative considered: runtime branching under always-compiled code. Rejected because AC3 requires compile-time isolation.

## Risks / Trade-offs

- [Risk] Missing one mutating path could bypass AC2 sync. -> Mitigation: enforce sync only through updateStorageFromCache and audit all mutators to use it.
- [Risk] AS write latency can affect write-heavy flows. -> Mitigation: keep file operations bounded and monitor logs for repeated failures.
- [Risk] Failure signals may be insufficient for operational tracking. -> Mitigation: include structured logs and telemetry marker coverage in validation evidence.

## Migration Plan

1. Verify updateStorageFromCache success branch triggers AC2 sync when feature flag is ON.
2. Audit mutating methods (init reconcile writes, setAutoConnect, setLastConnectTimeUtc, addDevice, removeDevice, and equivalent writers) to confirm they funnel through updateStorageFromCache.
3. Ensure AS sync failure path logs and telemetry emission points are present and non-rollback policy is retained.
4. Validate ON/OFF behavior and gather AC2 evidence for CPESP-9452.

## Open Questions

- None. AC2 failure-policy and compile-time gating expectations are already resolved by CPESP-9452 checklist decisions.
