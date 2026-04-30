## Why

Bluetooth migration and rollback requirements are currently spread across multiple specs that still reference outdated names and pre-consolidation assumptions (for example `BLUETOOTH_ENABLE_AS_MIGRATION`). This creates ambiguity during implementation and review because the codebase has already moved to `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` and a merged migration behavior model.

## What Changes

- Define a new consolidated capability `bluetooth-migration` that captures the current-state migration/rollback contract as implemented today.
- Consolidate AC1 migration, AC2 rollback sync, compile-time gating, adapter integration, and regression expectations into one normative spec for the active behavior.
- Align terminology in requirements and scenarios to current identifiers (`BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION`, current helper names, current persistence semantics).
- Deprecate overlapping or stale requirements in legacy migration-related specs by replacing them with references to the consolidated capability.
- Keep observability and release-governance requirements in their dedicated capabilities unless they require direct wording updates for identifier alignment.

## Capabilities

### New Capabilities
- `bluetooth-migration`: Canonical, current-state requirements for migration import, rollback synchronization, compile-time gating, and baseline behavior when migration support is disabled.

### Modified Capabilities
- `bluetooth-feature-gating`: Replace outdated migration flag name and reduce duplicate requirements that move into `bluetooth-migration`.
- `ac1-migration-path`: Mark split AC1 requirements as superseded by consolidated migration capability.
- `ac2-rollback-sync-path`: Mark split AC2 requirements as superseded by consolidated migration capability.
- `bluetooth-as-persistence-adapter`: Retain adapter-specific constraints while removing duplicate migration contract statements.
- `bluetooth-as-file-update-method-rename`: Align guard-name references to current compile-time flag naming.
- `bluetooth-tests-regression-coverage`: Align scenario wording and capability references to the consolidated migration contract.
- `bluetooth-release-deprecation-readiness`: Align deprecation-trigger wording to current migration flag naming.

## Impact

- OpenSpec artifacts under `openspec/specs/` for migration-related capabilities.
- Future implementation and validation work that consumes migration requirements and terminology.
- Review and release-readiness workflows by providing one canonical source for active migration behavior.
- No direct runtime behavior changes are intended in this change; this is a specification consolidation and alignment effort.
