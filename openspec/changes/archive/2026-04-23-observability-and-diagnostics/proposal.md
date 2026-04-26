## Why

CPESP-9452 section 7 requires explicit observability coverage for migration and rollback behavior, but current logging and telemetry semantics are not fully standardized for validation evidence. Locking structured diagnostics now improves triage speed and makes AC1/AC2 behavior verifiable across environments.

## What Changes

- Add structured migration diagnostics for start, success, and failure outcomes in Bluetooth device persistence flows.
- Add structured rollback synchronization diagnostics for success and failure outcomes after persistence updates.
- Add telemetry emission points for migration and rollback lifecycle markers.
- Define scenario-to-signal mapping so validation runs can directly correlate logs and telemetry with acceptance criteria.
- Add evidence template guidance for collecting raw observability output during section-7 validation.

## Capabilities

### New Capabilities
- `bluetooth-observability-diagnostics`: Defines required structured logs and telemetry markers for CPESP-9452 migration and rollback paths.

### Modified Capabilities
- None.

## Impact

- Affected implementation:
  - Bluetooth/BluetoothDeviceManager.cpp
- Affected validation artifacts:
  - openspec/changes/observability-and-diagnostics/tasks.md
  - section-7 evidence capture files under the same change directory
- Behavioral impact:
  - no API contract changes; diagnostics and telemetry only
- Operational impact:
  - improved runtime traceability for AC1/AC2 success and failure scenarios
