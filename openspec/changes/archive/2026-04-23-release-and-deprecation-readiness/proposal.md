## Why

Section 9 of CPESP-9452 requires explicit release-readiness and deprecation criteria so the temporary AC1/AC2 migration path can be retired safely without ambiguity. This is needed now to prevent long-lived feature-flag debt and to align implementation evidence with merge and release gates.

## What Changes

- Define and document resolved policy decisions in bluetooth design/spec artifacts for the migration window:
- deviceAddr is the Bluetooth MAC address.
- lastConnectionTimeUTC representation is schema-driven and consistent across AS file and PersistentStore.
- lastVolumeSetting authority remains BTMgr.
- AS-file write failures are non-fatal and handled as log-only diagnostics for this implementation track.
- Add explicit deprecation trigger criteria for removing BLUETOOTH_ENABLE_AS_MIGRATION and its AC1/AC2 ticket path.
- Define required validation evidence and review gates that must be met before deprecation path removal.

## Capabilities

### New Capabilities
- bluetooth-release-deprecation-readiness: Release policy and deprecation criteria for the CPESP-9452 migration/rollback feature-flagged path.

### Modified Capabilities
- None.

## Impact

- Documentation artifacts under specs/bluetooth-plugin are updated to serve as release and deprecation reference.
- OpenSpec change artifacts capture normative requirements and executable implementation tasks for Section 9.
- No functional runtime behavior changes are introduced by this proposal alone; this change governs readiness criteria and lifecycle policy.
