## ADDED Requirements

### Requirement: Field mapping table SHALL be explicitly defined and authoritative
The project SHALL define an explicit mapping table between AS persistence fields and plugin model fields, including conversion behavior, in specification artifacts.

#### Scenario: Mapping table exists in spec
- **WHEN** reviewers inspect mapping rules for AC1 and AC2
- **THEN** the spec contains a complete field-by-field mapping table with conversion notes

### Requirement: Resolved identity and representation decisions SHALL be enforced
Mapping rules SHALL encode resolved CPESP-9452 decisions: deviceAddr is Bluetooth MAC identity, lastConnectionTimeUTC representation is schema-consistent across local file and PersistentStore, and lastVolumeSetting is BTMgr-authoritative.

#### Scenario: Policy checks in mapping rules
- **WHEN** field mapping rules are reviewed
- **THEN** all resolved policy decisions are present and unambiguous

### Requirement: Spec and docs SHALL remain consistent for mapping semantics
The same mapping names, value semantics, and conversion notes SHALL be reflected consistently across spec and Bluetooth documentation artifacts.

#### Scenario: Cross-document consistency review
- **WHEN** spec and docs are compared for mapping content
- **THEN** no conflicting names, types, or conversion rules are present

### Requirement: Adapter failure-handling policy SHALL be documented with mapping rules
Design notes SHALL document adapter failure-handling policy in alignment with mapping rules used by migration and rollback paths.

#### Scenario: Design notes include mapping-related failure policy
- **WHEN** design notes are reviewed
- **THEN** adapter mapping decisions and failure-handling behavior are both documented and aligned
