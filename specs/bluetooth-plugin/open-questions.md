# Bluetooth Plugin Open Questions

This file has been re-processed with provided decisions.

## Resolved Decisions

1. Error Contract Standardization
- Strict per-method Thunder error-code mapping table is not required.
- Common normalized failure response shape beyond existing success semantics is not required.

2. Persistence Schema Governance
- autoconnect enum numeric encoding is fixed for backward compatibility.

3. API Surface Governance
- Spec-driven generation is the source of truth for method/event lists.

4. Section 9 Policy Decisions (Release/Deprecation Readiness)
- deviceAddr is the canonical Bluetooth MAC address across AS and PersistentStore views.
- lastConnectionTimeUTC representation follows the schema definition consistently across both stores.
- lastVolumeSetting authority remains BTMgr.
- AS-file write failures in the gated migration window are non-fatal and handled through structured logs under the active implementation track.
- BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION path removal requires evidence-complete readiness plus explicit owner/governance/QA approvals.

## Remaining Open Questions

1. Persistence Schema Evolution
- Additional metadata fields in upcoming releases: Maybe.
- Policy needed: define additive-field governance (optional fields, backward compatibility, reader/writer tolerance).

2. Discovery Stop Operation Type
- Current behavior: `BTRMGR_StopDeviceDiscovery` always passes `AUDIO_OUTPUT` regardless of the operation type used to start discovery.
- Question: Does BTMgr treat the stop operation type as selective (stopping only that class's scan) or as a no-op field (stopping all active discovery)?
- Impact: If selective, stopping after an HID or LE scan with `AUDIO_OUTPUT` may silently fail to stop the scan.
- Decision needed: Ratify the current `AUDIO_OUTPUT` hardcoding as intentional, or require that stop tracks the operation type used at start.

3. DEVICE_DISCONNECT_FAILED Silent Drop
- Current behavior: BTMgr `DEVICE_DISCONNECT_FAILED` events are silently dropped with no client notification.
- Question: Should a failed disconnection be observable to clients (e.g., via `onRequestFailed`)?
- Impact: During power-mode managed-disconnect sequences, a client has no way to know that a disconnect attempt failed.
- Decision needed: Ratify the current silence as intentional, or add `onRequestFailed(CONNECTION_FAILED)` emission for this event.

4. RESTART Audio Playback Command
- Current behavior: `sendAudioPlaybackCommand` with `RESTART` returns `BTRMGR_RESULT_GENERIC_FAILURE` with a TODO comment in code.
- Question: Is `RESTART` permanently unsupported and should be formally deprecated, or is it a gap to be implemented?
- Decision needed: Mark `RESTART` as deprecated in the API surface, or define the expected BTMgr mapping and implement it.
