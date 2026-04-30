## 1. Consolidated Capability Definition

- [x] 1.1 Create `bluetooth-migration` delta spec with canonical requirements for initialization import, rollback synchronization, compile-time gating, and baseline behavior.
- [x] 1.2 Verify each canonical requirement includes at least one testable `#### Scenario` using normative SHALL/MUST language.
- [x] 1.3 Confirm consolidated capability terminology matches current implementation identifiers (`BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`, current helper naming).

## 2. Legacy Capability Supersession And Alignment

- [x] 2.1 Add REMOVED requirement deltas for `ac1-migration-path` with explicit `Reason` and `Migration` references to `bluetooth-migration`.
- [x] 2.2 Add REMOVED requirement deltas for `ac2-rollback-sync-path` with explicit `Reason` and `Migration` references to `bluetooth-migration`.
- [x] 2.3 Add MODIFIED deltas for active capabilities (`bluetooth-feature-gating`, `bluetooth-as-persistence-adapter`, `bluetooth-as-file-update-method-rename`, `bluetooth-tests-regression-coverage`, `bluetooth-release-deprecation-readiness`) to align current flag naming and remove stale wording.

## 3. Consistency Validation

- [x] 3.1 Validate every capability listed in proposal.md has a corresponding delta spec file under this change.
- [x] 3.2 Run repository search across active specs to identify remaining `BLUETOOTH_ENABLE_AS_MIGRATION` references and either update or explicitly justify retained mentions.
- [x] 3.3 Ensure no modified delta uses partial requirement blocks where full MODIFIED content is required.

## 4. Apply-Readiness Checks

- [x] 4.1 Run `openspec status --change "consolidate-bluetooth-migration-spec" --json` and confirm proposal/design/specs/tasks all report `done`.
- [x] 4.2 Run `openspec status --change "consolidate-bluetooth-migration-spec"` and confirm the change is ready for `/opsx:apply`.
- [x] 4.3 Capture any follow-up cleanup candidates (for example archival cleanup of superseded split capabilities) in implementation notes if not executed in this change.
