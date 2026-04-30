## ADDED Requirements

### Requirement: AS persistence adapter reads and maps migration source data
The Bluetooth AS persistence adapter SHALL read /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json, validate payload structure, and map supported schema fields into plugin persistence model values without crashing on malformed entries.

#### Scenario: Read succeeds with valid schema payload
- **WHEN** the adapter reads an AS payload containing a pairedDevices array with valid entries
- **THEN** it returns success and provides mapped device records for migration

#### Scenario: Read handles malformed payload safely
- **WHEN** the adapter receives malformed JSON or missing pairedDevices array
- **THEN** it returns a non-success result and no crash occurs in caller flow

### Requirement: Adapter applies tolerant parse fallback for legacy type variants
The adapter SHALL support legacy-compatible coercion for lastConnectionTimeUTC and autoConnectStatus while preserving deterministic mapping behavior.

#### Scenario: Numeric timestamp is accepted
- **WHEN** lastConnectionTimeUTC is encoded as a number in AS payload
- **THEN** the mapped plugin field stores an equivalent canonical string value

#### Scenario: String timestamp is accepted
- **WHEN** lastConnectionTimeUTC is encoded as a numeric string in AS payload
- **THEN** the mapped plugin field stores that canonical timestamp value

#### Scenario: Boolean coercion variants are accepted
- **WHEN** autoConnectStatus is provided as boolean, numeric, or string truthy values
- **THEN** the mapped plugin auto-connect status reflects equivalent enabled or disabled semantics

### Requirement: Adapter writes schema-conformant AS payload atomically
The adapter SHALL serialize paired device data to schema-conformant AS payload and write it using temp-file-and-rename atomic semantics.

#### Scenario: Successful write updates destination atomically
- **WHEN** adapter write is invoked with a valid cache snapshot
- **THEN** it writes to a temporary file and renames to destination as a single successful update

#### Scenario: Write failure does not corrupt destination
- **WHEN** temporary write or rename fails
- **THEN** adapter returns non-success and leaves caller responsible for non-rollback policy

### Requirement: Adapter integration remains compile-time isolated
All adapter compilation units and call sites for CPESP-9452 SHALL be excluded when BLUETOOTH_ENABLE_AS_MIGRATION is disabled.

#### Scenario: Build with migration flag OFF excludes adapter path
- **WHEN** Bluetooth is built with BLUETOOTH_ENABLE_AS_MIGRATION disabled
- **THEN** adapter code is not compiled or linked into migration and rollback call paths
