# CPESP-9452 Section 5 Validation Evidence

Change: define-and-lock-field-mapping-rules
Date: 2026-04-22
Owner: <fill>

## Objective
Capture raw outputs proving field mapping rules are defined and consistent across spec and docs artifacts.

## Artifact References
- specs/bluetooth-plugin/spec.md
- specs/bluetooth-plugin/design-notes.md
- docs/bluetooth-plugin.md

## Raw Command Outputs
Paste raw terminal outputs below exactly as produced.

### 1) Mapping Table Presence (Spec)
Command:
```bash
rg -n "AS Field|deviceAddr|paired_at|lastConnectionTimeUTC" specs/bluetooth-plugin/spec.md
```
Output:
```text
<paste raw output>
```

### 2) Mapping Table Presence (Docs)
Command:
```bash
rg -n "Migration and rollback field mapping|deviceAddr|paired_at|lastConnectionTimeUTC" docs/bluetooth-plugin.md
```
Output:
```text
<paste raw output>
```

### 3) Design Policy Coverage
Command:
```bash
rg -n "Policy locks|Failure-Handling Policy|BTMgr-authoritative" specs/bluetooth-plugin/design-notes.md
```
Output:
```text
<paste raw output>
```

### 4) Cross-File Consistency Spot Check
Command:
```bash
rg -n "deviceAddr|friendlyName|deviceType|lastVolumeSetting|autoConnectStatus|lastConnectionTimeUTC|paired_at" specs/bluetooth-plugin/spec.md specs/bluetooth-plugin/design-notes.md docs/bluetooth-plugin.md
```
Output:
```text
<paste raw output>
```

## Reviewer Sign-off
- [ ] Mapping names consistent
- [ ] Conversion semantics consistent
- [ ] Policy locks captured
- [ ] Evidence complete
