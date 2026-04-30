## Why

Section 8 of CPESP-9452 requires explicit non-functional hardening for AS-file persistence and migration initialization paths. Without bounded I/O behavior and concurrency protections, concurrent updates and malformed source data can cause instability or unreliable recovery behavior.

## What Changes

- Define concurrency safeguards for AS-file read/write interaction during rollback synchronization and migration.
- Define bounded boot-path behavior so initialization remains resilient and does not over-block boot-critical flow.
- Strengthen malformed AS-data resilience expectations so initialization cannot crash on invalid payloads.
- Add validation evidence guidance for stress/repeat scenarios to detect corruption and race regressions.

## Capabilities

### New Capabilities
- `bluetooth-non-functional-safeguards`: Defines concurrency, bounded-I/O, and malformed-data resilience requirements for CPESP-9452 migration and rollback paths.

### Modified Capabilities
- None.

## Impact

- Affected implementation:
  - Bluetooth/BluetoothAsPersistenceAdapter.cpp
  - Bluetooth/BluetoothDeviceManager.cpp
- Affected validation assets:
  - section-8 tasks and evidence files under openspec/changes/non-functional-safeguards/
- Runtime impact:
  - improved robustness under concurrent writes and malformed input conditions without API changes
