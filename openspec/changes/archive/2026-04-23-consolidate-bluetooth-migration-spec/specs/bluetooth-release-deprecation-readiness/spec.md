## MODIFIED Requirements

### Requirement: Deprecation Criteria For Migration Flag Path Must Be Defined
The project SHALL define objective deprecation trigger criteria for removing BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION migration-path code.

#### Scenario: Deprecation criteria are actionable
- **WHEN** release maintainers evaluate whether to remove BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION path code
- **THEN** criteria SHALL include required validation evidence for migration import, rollback synchronization, and baseline behavior with migration disabled
- **THEN** criteria SHALL include non-functional safeguard evidence demonstrating no corruption or crash regressions
- **THEN** criteria SHALL include approval gates identifying who can authorize removal
- **THEN** criteria SHALL require a fallback decision to retain the path when evidence is incomplete or inconclusive

### Requirement: Release Readiness Must Gate Path Removal
Removal of compile-time-gated migration code SHALL occur only after release-readiness criteria are satisfied and approved.

#### Scenario: Readiness criteria not satisfied
- **WHEN** evidence or approvals are missing at release cut
- **THEN** BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION path SHALL remain available
- **THEN** deprecation SHALL be deferred to a subsequent change

#### Scenario: Readiness criteria satisfied
- **WHEN** evidence and approvals satisfy Section 9 criteria
- **THEN** a dedicated follow-up change SHALL be created to remove BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION path code
- **THEN** that removal change SHALL include regression verification scope for remaining Bluetooth persistence behavior
