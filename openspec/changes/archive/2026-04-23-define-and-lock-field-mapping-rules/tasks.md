## 1. Spec Mapping Table Definition

- [x] 1.1 Add explicit AS-to-plugin field mapping table in specs/bluetooth-plugin/spec.md.
- [x] 1.2 Document conversion semantics for deviceAddr, friendlyName, deviceType, lastVolumeSetting, autoConnectStatus, and lastConnectionTimeUTC.
- [x] 1.3 Record resolved policy decisions in spec language: deviceAddr as Bluetooth MAC, schema-consistent timestamp representation, and BTMgr authority for lastVolumeSetting.

## 2. Documentation Alignment

- [x] 2.1 Add migration and rollback mapping section in docs/bluetooth-plugin.md using the same table semantics.
- [x] 2.2 Add adapter mapping decisions and failure-handling policy in specs/bluetooth-plugin/design-notes.md.
- [x] 2.3 Ensure mapping terminology and type expectations match exactly across spec and docs artifacts.

## 3. Consistency Validation

- [x] 3.1 Perform cross-file consistency review for mapping field names and conversion rules.
- [x] 3.2 Confirm no conflicting type/name semantics remain between spec and docs.
- [x] 3.3 Capture final mapping-rule evidence and references for CPESP-9452 section 5 closure.
