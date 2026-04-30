## Context

CPESP-9452 AC1 and AC2 introduce migration and rollback behaviors that depend on an AS-file persistence source at /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json. Current BluetoothDeviceManager logic mixes persistence orchestration with business logic, which increases coupling and makes parse/write error handling harder to validate. The design must preserve compile-time isolation using BLUETOOTH_ENABLE_AS_MIGRATION and keep baseline Bluetooth behavior unchanged when disabled.

## Goals / Non-Goals

**Goals:**
- Define a dedicated adapter boundary for AS-file read, parse, map, and write operations.
- Keep migration and rollback call sites explicit in BluetoothDeviceManager while delegating file semantics to the adapter.
- Enforce tolerant parse and non-fatal failure policy for initialization and sync paths.
- Preserve schema-aligned field handling and atomic write behavior.

**Non-Goals:**
- Rework baseline Bluetooth persistence architecture beyond AC1/AC2 scope.
- Introduce new runtime feature toggles or CI matrix targets in this change.
- Redefine product-level data ownership outside currently resolved CPESP-9452 decisions.

## Decisions

- Adapter boundary in Bluetooth module:
  - Decision: Introduce BluetoothAsPersistenceAdapter as the only owner of AS-file parse/serialize and file-write mechanics.
  - Rationale: Centralizes schema handling and failure semantics, reducing repeated error-prone logic in manager flows.
  - Alternative considered: Keep file parsing directly in BluetoothDeviceManager. Rejected due to coupling and reduced testability.

- Failure semantics via explicit return codes:
  - Decision: Adapter methods return Core::hresult to distinguish missing source, invalid data, and generic failures.
  - Rationale: Enables deterministic fallback policy in init and rollback paths without conflating benign absence with corruption.
  - Alternative considered: bool success/failure. Rejected because it hides actionable fallback distinctions.

- Schema-tolerant read and normalized write:
  - Decision: Read path accepts numeric/string timestamp forms and boolean coercion variants; write path emits normalized schema-conformant values and preserves safe existing values where possible.
  - Rationale: Supports upgrade compatibility while converging outputs to source-of-truth schema shape.
  - Alternative considered: strict reject on non-exact types. Rejected due to migration fragility risk.

- AC3 compile-time isolation:
  - Decision: Adapter build inclusion and migration/sync call sites remain guarded by BLUETOOTH_ENABLE_AS_MIGRATION.
  - Rationale: Allows ticket path removal readiness and guarantees OFF-mode exclusion.
  - Alternative considered: runtime-only gating. Rejected because AC3 requires compile-time isolation.

## Risks / Trade-offs

- [Risk] Host filesystem constraints in L1 environments may limit AS file write/read setup. -> Mitigation: tests skip gracefully when path setup is not possible and retain core baseline coverage.
- [Risk] Tolerant coercion could accept unexpected legacy values. -> Mitigation: constrain coercion to explicit numeric/string/boolean conversions and default-safe fallbacks.
- [Risk] Additional migration logging may increase verbosity. -> Mitigation: use structured and scenario-specific log messages.

## Migration Plan

1. Add adapter interface and implementation under Bluetooth feature gate.
2. Integrate adapter calls in BluetoothDeviceManager init and post-persist sync points.
3. Add L1 coverage for fallback parsing edge cases and compile-gate behavior.
4. Validate ON/OFF build and runtime behavior in provisioned environment and capture artifacts.

## Open Questions

- None for this scoped change. Policy decisions for schema authority, failure handling, and compile-time gating are already resolved in CPESP-9452 artifacts.
