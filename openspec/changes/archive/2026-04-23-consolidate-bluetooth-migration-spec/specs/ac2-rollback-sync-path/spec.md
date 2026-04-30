## REMOVED Requirements

### Requirement: AC2 rollback sync SHALL occur after successful PersistentStore writes
**Reason**: Rollback synchronization sequencing is now consolidated in the canonical `bluetooth-migration` capability.
**Migration**: Use `bluetooth-migration` requirement "Rollback synchronization SHALL run after successful persistence writes".

### Requirement: AC2 sync SHALL cover all mutating persistence paths
**Reason**: Shared mutating-path synchronization behavior is now part of consolidated migration semantics.
**Migration**: Use `bluetooth-migration` requirement "Rollback synchronization SHALL run after successful persistence writes".

### Requirement: AS sync failure SHALL not roll back PersistentStore success
**Reason**: Non-rollback failure policy is maintained in one canonical migration requirement set.
**Migration**: Use `bluetooth-migration` requirement "Rollback synchronization SHALL run after successful persistence writes".

### Requirement: AC2 rollback path SHALL be compile-time isolated
**Reason**: Compile-time isolation is consolidated using current flag naming in canonical migration/gating capabilities.
**Migration**: Use `bluetooth-migration` requirement "Compile-time migration gating SHALL use canonical flag naming".
