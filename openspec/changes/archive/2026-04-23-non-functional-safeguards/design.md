## Context

Section 8 of CPESP-9452 focuses on non-functional hardening for AS-file migration and rollback synchronization paths. Current implementation already supports migration fallback behavior and atomic rename writes, but section-8 acceptance requires explicit safeguards for concurrent writes, bounded boot-path I/O impact, and crash-safe malformed-data handling with stress/repeat validation guidance.

## Goals / Non-Goals

**Goals:**
- Define concurrency safeguards that prevent AS-file corruption under repeated mutating operations.
- Define bounded boot-path behavior so initialization remains resilient even when AS source files are slow or malformed.
- Define malformed-data resilience expectations that guarantee no initialization crash.
- Define section-8 validation evidence expectations for stress/repeat safety.

**Non-Goals:**
- Add new persistence formats or schema fields.
- Redesign migration/rollback control flow beyond safeguards required for reliability.
- Introduce broad architectural changes outside BluetoothAsPersistenceAdapter and BluetoothDeviceManager.

## Decisions

- Serialization and write integrity hardening:
  - Decision: retain temp-file plus atomic rename semantics and explicitly enforce single-writer behavior for AS file updates.
  - Rationale: prevents partial-write corruption under repeated updates.
  - Alternative considered: direct in-place writes. Rejected due to corruption risk on interruption.

- Boot-path bounded behavior:
  - Decision: migration read/parse failures remain non-fatal and must not block initialization completion beyond bounded file I/O.
  - Rationale: startup resilience is required in constrained environments.
  - Alternative considered: strict migration success requirement. Rejected because it can degrade boot reliability.

- Malformed input safety:
  - Decision: malformed AS payloads are handled with tolerant parse failure reporting and fallback, without process termination.
  - Rationale: section-8 explicitly requires crash prevention.
  - Alternative considered: hard-fail on malformed input. Rejected due to availability impact.

## Risks / Trade-offs

- [Risk] Additional synchronization may increase contention in high-frequency mutation paths. -> Mitigation: keep lock scope limited to AS write critical sections.
- [Risk] Strict bounded-I/O guarantees are environment-sensitive. -> Mitigation: define practical validation thresholds and collect external run evidence.
- [Risk] Stress behavior may vary with filesystem characteristics. -> Mitigation: require repeat-run evidence and corruption checks in validation artifacts.

## Migration Plan

1. Add/verify concurrency guard around AS-file write path and ensure no cross-thread corruption path remains.
2. Confirm migration initialization path remains non-fatal on malformed/unavailable AS source with bounded file operations.
3. Add section-8 evidence template for stress/repeat and corruption checks.
4. Validate safeguards in external stress runs and capture raw outputs.

## Open Questions

- Whether platform-specific filesystems require additional fsync/durability tuning beyond current atomic-rename semantics.
