# Changelog

All notable changes to this RDK Service will be documented in this file.

* Each RDK Service has a CHANGELOG file that contains all changes done so far. When version is updated, add a entry in the CHANGELOG.md at the top with user friendly information on what was changed with the new version. Please don't mention JIRA tickets in CHANGELOG. 

* Please Add entry in the CHANGELOG for each version change and indicate the type of change with these labels:
    * **Added** for new features.
    * **Changed** for changes in existing functionality.
    * **Deprecated** for soon-to-be removed features.
    * **Removed** for now removed features.
    * **Fixed** for any bug fixes.
    * **Security** in case of vulnerabilities.

* Changes in CHANGELOG should be updated when commits are added to the main or release branches. There should be one CHANGELOG entry per JIRA Ticket. This is not enforced on sprint branches since there could be multiple changes for the same JIRA ticket during development. 

## [Unreleased]
### Added
- Added explicit PersistentStore migration marker (`migrationVersion`) as the sole source of migration truth.
- Added `performMigration` JSON-RPC method: clears stale store data, imports from AS storage, enriches via BTRMGR, writes RDK store, then writes migration marker.
- Added `clearMigration` JSON-RPC method: removes store data and migration marker, clears RAM cache, preserves AS storage.
### Changed
- `init()` no longer auto-migrates or reads AS at startup; migration state is determined only by the migration marker.
- All persistence APIs (`setAutoConnect`, `addDevice`, `removeDevice`, etc.) are no-ops before migration completes.
- `getAutoConnect` returns disabled before migration completes.

## [1.0.11] - 2025-01-27
### Added
-  Make sure connect response is sent on a connect request
