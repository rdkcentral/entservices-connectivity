## MODIFIED Requirements

### Requirement: AC2 Rollback Semantics Must Be Preserved Through Rename
Renaming the helper SHALL NOT change AC2 rollback execution behavior.

#### Scenario: Invocation ordering unchanged
- **WHEN** device metadata persistence succeeds in updateStorageFromCache
- **THEN** updateAsFileFromCache SHALL be invoked in the success path under BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION
- **THEN** AS write failures SHALL remain non-fatal and SHALL NOT roll back PersistentStore updates
