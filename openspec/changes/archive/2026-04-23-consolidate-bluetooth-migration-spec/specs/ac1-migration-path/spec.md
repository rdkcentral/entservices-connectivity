## REMOVED Requirements

### Requirement: Initialization SHALL prioritize Bluetooth PersistentStore before AS migration
**Reason**: Initialization migration behavior is now defined canonically in `bluetooth-migration` to avoid split/duplicate migration contracts.
**Migration**: Use `bluetooth-migration` requirement "Migration initialization SHALL follow store-first import semantics".

### Requirement: Initialization SHALL import from AS only when store data is absent
**Reason**: Store-absent import behavior is consolidated in the canonical migration capability.
**Migration**: Use `bluetooth-migration` requirement "Migration initialization SHALL follow store-first import semantics".

### Requirement: Initialization SHALL remain non-fatal on AS migration read or parse failure
**Reason**: Fallback and non-fatal behavior is now maintained in one canonical migration requirement set.
**Migration**: Use `bluetooth-migration` requirement "Migration initialization SHALL follow store-first import semantics".

### Requirement: AC1 migration path SHALL be compile-time isolated
**Reason**: Compile-time gating requirements are consolidated under current naming in `bluetooth-migration` and `bluetooth-feature-gating`.
**Migration**: Use `bluetooth-migration` requirement "Compile-time migration gating SHALL use canonical flag naming".
