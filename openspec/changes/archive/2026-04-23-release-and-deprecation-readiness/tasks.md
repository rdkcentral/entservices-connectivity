## 1. Policy Documentation Alignment

- [x] 1.1 Update specs/bluetooth-plugin/design-notes.md with Section 9 resolved policies for deviceAddr, lastConnectionTimeUTC representation, and lastVolumeSetting authority.
- [x] 1.2 Update specs/bluetooth-plugin/open-questions.md to record final Section 9 decisions and close related open items.
- [x] 1.3 Record AS-file write failure handling policy as non-fatal structured logging behavior consistent with active implementation track.

## 2. Deprecation Trigger Definition

- [x] 2.1 Define explicit trigger criteria for removing BLUETOOTH_ENABLE_AS_MIGRATION AC1/AC2 path in specs/bluetooth-plugin/design-notes.md.
- [x] 2.2 Define required evidence and approval gates for path removal, including fallback-to-retain behavior when criteria are unmet.
- [x] 2.3 Document required follow-up change scope for actual code removal once criteria are satisfied.

## 3. Readiness Validation And Traceability

- [x] 3.1 Create section-9 readiness evidence template under this change directory for release-review sign-off.
- [x] 3.2 Add checklist traceability notes linking Section 9 criteria to AC2 and AC3 governance obligations.
- [x] 3.3 Run openspec instructions apply --change "release-and-deprecation-readiness" --json and confirm all tasks are complete.
