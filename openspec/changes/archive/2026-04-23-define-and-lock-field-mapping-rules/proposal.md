## Why

Field mapping rules between AS persistence payloads and plugin persistence model are currently implicit across code and docs, which creates risk of drift and inconsistent behavior. Locking these rules now provides a stable contract for AC1/AC2 behavior, test expectations, and future deprecation decisions.

## What Changes

- Define an explicit field mapping table for AS fields to plugin model fields and conversion rules.
- Document and enforce resolved policy decisions: deviceAddr as Bluetooth MAC address, schema-consistent lastConnectionTimeUTC representation, and BTMgr authority for lastVolumeSetting.
- Add aligned migration/rollback mapping documentation in Bluetooth docs.
- Add design-notes coverage for adapter mapping and failure-handling policy so implementation and docs use one canonical interpretation.

## Capabilities

### New Capabilities
- bluetooth-field-mapping-rules: establishes authoritative mapping and conversion contract for AS and plugin persistence fields used by migration and rollback paths.

### Modified Capabilities
- None.

## Impact

- Affected artifacts: specs/bluetooth-plugin/spec.md, docs/bluetooth-plugin.md, specs/bluetooth-plugin/design-notes.md.
- Behavioral impact: clarifies expected serialization and conversion behavior across AC1 and AC2 paths.
- Validation impact: reduces ambiguity in tests and evidence by giving one source of truth for field semantics.
