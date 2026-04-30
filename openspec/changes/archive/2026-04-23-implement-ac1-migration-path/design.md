## Context

CPESP-9452 AC1 requires Bluetooth initialization to migrate device metadata from the AS persistence file only when Bluetooth PersistentStore data is missing. Existing behavior must remain stable for store-present startup paths and must not fail initialization when AS data is unavailable or malformed. The migration path is controlled by the compile-time feature flag BLUETOOTH_ENABLE_AS_MIGRATION.

## Goals / Non-Goals

**Goals:**
- Define initialization flow rules for PersistentStore-first and AS-fallback behavior.
- Ensure migration import is non-fatal and continues with existing device reconciliation when AS data cannot be used.
- Keep AC1 logic isolated under compile-time gating and scoped to BluetoothDeviceManager migration orchestration.

**Non-Goals:**
- Introduce AC2 rollback synchronization behavior changes.
- Add new runtime toggles or alter platform-wide build policy.
- Redesign Bluetooth persistent model outside AC1 initialization scope.

## Decisions

- PersistentStore-first startup:
  - Decision: init checks Bluetooth PersistentStore first and skips AS import when store data exists.
  - Rationale: avoids overwriting authoritative persisted data and preserves baseline behavior.
  - Alternative considered: always import AS first. Rejected due to potential data regression.

- AS import only on store absence:
  - Decision: migration from AS occurs only when store GetValue returns not-exist.
  - Rationale: AC1 specifically targets bootstrap migration, not routine refresh.
  - Alternative considered: import on any store read failure. Rejected because transient errors should not trigger migration.

- Non-fatal migration failure policy:
  - Decision: AS missing/invalid payload logs and falls back to reconcile flow without failing init.
  - Rationale: plugin startup reliability is prioritized over migration completeness.
  - Alternative considered: fail init when migration fails. Rejected due to service availability risk.

- Compile-time isolation:
  - Decision: migration orchestration declarations and usages remain under BLUETOOTH_ENABLE_AS_MIGRATION.
  - Rationale: ensures AC1 path can be fully excluded in OFF-mode builds.
  - Alternative considered: runtime gating only. Rejected as insufficient for AC3 isolation objectives.

## Risks / Trade-offs

- [Risk] Store-read error categories may be misinterpreted as migration triggers. -> Mitigation: trigger migration only on explicit not-exist condition.
- [Risk] Malformed AS payload can silently skip migration. -> Mitigation: log structured failure and continue reconcile path.
- [Risk] AC1-only scope may duplicate some logic with broader AC2 workstreams. -> Mitigation: keep adapter boundaries and call points consistent with existing adapter design.

## Migration Plan

1. Add and confirm AC1 migration helper declarations in BluetoothDeviceManager header under compile-time guard.
2. Implement init branching for store present vs absent behavior.
3. Call AS adapter read/import and persist imported cache when store is absent.
4. Keep failure path non-fatal and proceed to existing updateCacheFromDevice plus updateStorageFromCache flow.
5. Validate AC1 scenarios with evidence for store-present and store-absent cases.

## Open Questions

- None. AC1 path criteria and failure policy are already defined in CPESP-9452 checklist scope for section 3.
