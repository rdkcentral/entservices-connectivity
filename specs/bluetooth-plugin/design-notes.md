# Bluetooth Plugin Design Notes

## Architecture Snapshot
- Thunder JSON-RPC plugin class: Bluetooth
- Metadata persistence helper: BluetoothDeviceManager
- External dependencies: BTRMGR, org.rdk.PersistentStore, org.rdk.PowerManager

## Data Ownership
- Runtime adapter/device operations are owned by Bluetooth (BTRMGR bridge).
- Persisted paired-device metadata is owned by BluetoothDeviceManager.

## Persisted Data Model
PersistentStore namespace/key:
- Namespace: Bluetooth
- Key: deviceInfo

Each device entry persists:
- deviceAddr: string (Bluetooth MAC identity)
- friendlyName: string
- deviceType: string
- lastVolumeSetting: integer (BTMgr-authoritative)
- autoConnectStatus: boolean
- lastConnectionTimeUTC: integer timestamp

## Adapter Failure-Handling Policy

Migration and rollback paths use resilient, non-fatal failure handling:
- AC1 migration is attempted only on PersistentStore miss (`ERROR_NOT_EXIST`).
- Missing filesystem persistence source or parse failure is non-fatal and MUST NOT block plugin initialization.
- Field-level parse failures preserve existing cache values when available.
- AC2 rollback sync runs after successful cache persistence; rollback write failures are logged and surfaced by error policy without corrupting in-memory cache.

Section 9 release policy lock:
- AS-file write failure handling for the CPESP-9452 gated path is non-fatal and structured-log based for the active implementation track.
- PersistentStore success remains authoritative; AS-file sync failure does not roll back cache or store updates.

## Release And Deprecation Readiness Criteria (Section 9)

BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION AC1/AC2 path removal is permitted only when all triggers below are satisfied:

1. Evidence completeness
- AC1 migration evidence is attached and validated for store-present, AS-import, missing-source, and malformed-source scenarios.
- AC2 rollback sync evidence is attached and validated for flag ON and OFF behavior.
- AC3 compile-time isolation evidence is attached and validated for flag ON and OFF builds.
- Non-functional safeguard evidence is attached and validated for bounded I/O handling and malformed-input resilience.

2. Approval gates
- Bluetooth plugin owner approval is recorded.
- Release governance approval is recorded.
- QA validation sign-off is recorded.

3. Fallback-to-retain rule
- If any evidence item or approval gate is missing, BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION path MUST be retained for the release.
- Path removal MUST be deferred to a dedicated follow-up change.

## Follow-Up Removal Change Scope

When section-9 triggers are satisfied, a dedicated removal change MUST include:
- Delete BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION guarded AC1/AC2 ticket-path code from Bluetooth device manager and filesystem persistence adapter integration points.
- Remove compile-time flag wiring and related build toggles where no longer needed.
- Preserve baseline Bluetooth persistence behavior and verify regression scope with L1 coverage.
- Update design/spec artifacts to remove temporary migration-path references and record final-state behavior.

## Checklist Traceability Notes (Section 9 -> AC2/AC3)

- AC2 governance link: path removal requires validated rollback-sync behavior and documented failure policy before deleting gated code.
- AC3 governance link: path removal requires compile-time isolation evidence proving both flag states were previously validated and removal does not regress baseline behavior.

## Notable Behavioral Decisions
- Adapter index is fixed to 0 for all BTRMGR operations.
- Initialization tolerates missing PowerManager by logging error and continuing plugin init path.
- Auto-connect status is optional in status events when unavailable for a device.
- Auto-connect numeric enum encoding is treated as backward-compatible persisted schema.
- API contract source-of-truth is spec-driven; docs should follow spec updates.

## Reliability Notes
- PersistentStore failures are surfaced via error return codes and logs.
- Cache operations are protected with a critical section.
- Event callback handling verifies plugin singleton instance before forwarding event.

## Future Enhancements (Exploration)
- Multi-adapter support and adapter selection strategy.
- Event payload schema formalization and compatibility rules.
- End-to-end conformance tests for all emitted events.
