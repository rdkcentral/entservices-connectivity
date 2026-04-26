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

## Remaining Open Question

1. Persistence Schema Evolution
- Additional metadata fields in upcoming releases: Maybe.
- Policy needed: define additive-field governance (optional fields, backward compatibility, reader/writer tolerance).
