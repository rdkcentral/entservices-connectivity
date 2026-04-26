## ADDED Requirements

### Requirement: Section 9 Policy Decisions Must Be Normatively Recorded
The project SHALL record Section 9 policy decisions in maintained bluetooth design/spec artifacts so release and maintenance decisions are based on explicit, versioned guidance.

#### Scenario: Policy decisions are documented
- **WHEN** Section 9 artifacts are reviewed for CPESP-9452 readiness
- **THEN** the artifacts SHALL state that deviceAddr is the Bluetooth MAC address
- **THEN** the artifacts SHALL state that lastConnectionTimeUTC representation follows the schema across AS file and PersistentStore
- **THEN** the artifacts SHALL state that lastVolumeSetting authority is BTMgr
- **THEN** the artifacts SHALL define AS-file write failure handling as non-fatal structured logging behavior for the active implementation track

### Requirement: Deprecation Criteria For Migration Flag Path Must Be Defined
The project SHALL define objective deprecation trigger criteria for removing BLUETOOTH_ENABLE_AS_MIGRATION AC1/AC2 ticket path code.

#### Scenario: Deprecation criteria are actionable
- **WHEN** release maintainers evaluate whether to remove BLUETOOTH_ENABLE_AS_MIGRATION path code
- **THEN** criteria SHALL include required validation evidence for AC1, AC2, and AC3 behavior
- **THEN** criteria SHALL include non-functional safeguard evidence demonstrating no corruption or crash regressions
- **THEN** criteria SHALL include approval gates identifying who can authorize removal
- **THEN** criteria SHALL require a fallback decision to retain the path when evidence is incomplete or inconclusive

### Requirement: Release Readiness Must Gate Path Removal
Removal of compile-time-gated AC1/AC2 ticket code SHALL occur only after release-readiness criteria are satisfied and approved.

#### Scenario: Readiness criteria not satisfied
- **WHEN** evidence or approvals are missing at release cut
- **THEN** BLUETOOTH_ENABLE_AS_MIGRATION path SHALL remain available
- **THEN** deprecation SHALL be deferred to a subsequent change

#### Scenario: Readiness criteria satisfied
- **WHEN** evidence and approvals satisfy Section 9 criteria
- **THEN** a dedicated follow-up change SHALL be created to remove BLUETOOTH_ENABLE_AS_MIGRATION path code
- **THEN** that removal change SHALL include regression verification scope for remaining Bluetooth persistence behavior
