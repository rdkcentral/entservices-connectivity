## ADDED Requirements

### Requirement: Initialization SHALL NOT auto-import migration data from the filesystem
Bluetooth initialization SHALL read PersistentStore device info to seed the in-memory cache and SHALL NOT automatically import migration data from the filesystem. Filesystem migration is deferred to an explicit client invocation of `performMigration()`.

#### Scenario: Initialization with existing PersistentStore data
- **WHEN** initialization finds valid Bluetooth/deviceInfo data in PersistentStore
- **THEN** the cache is seeded from PersistentStore
- **THEN** no migration import is attempted

#### Scenario: Initialization with no PersistentStore data
- **WHEN** initialization finds no Bluetooth/deviceInfo data in PersistentStore
- **THEN** the cache starts empty
- **THEN** no migration import is attempted
- **THEN** initialization continues with baseline reconciliation flow

### Requirement: `performMigration` SHALL use presence-based import semantics
The `performMigration()` API SHALL determine whether to import from the filesystem source solely by checking whether `deviceInfo` is already present in PersistentStore. If `deviceInfo` is present, migration has already occurred and the call is a no-op. If `deviceInfo` is absent, it is the first migration and the import proceeds.

#### Scenario: First call (deviceInfo absent from PersistentStore)
- **WHEN** `performMigration()` is called and `deviceInfo` is absent from PersistentStore
- **THEN** migration data is imported from the filesystem source into cache and persisted to PersistentStore

#### Scenario: Subsequent call (deviceInfo already present in PersistentStore)
- **WHEN** `performMigration()` is called and `deviceInfo` is already present in PersistentStore
- **THEN** filesystem migration import is skipped
- **THEN** `performMigration()` returns success indicating migration was already complete

#### Scenario: Missing filesystem source
- **WHEN** `performMigration()` is called and the filesystem source file does not exist
- **THEN** the missing file is treated as an empty payload (zero devices)
- **THEN** `performMigration()` proceeds with the empty payload and returns success (`ERROR_NONE`)

#### Scenario: Unreadable or malformed filesystem source
- **WHEN** `performMigration()` is called and the filesystem source file exists but cannot be read (e.g., I/O error) or cannot be parsed (malformed JSON)
- **THEN** `performMigration()` returns an error and aborts migration to prevent data loss

#### Scenario: deviceInfo presence check fails with unexpected error
- **WHEN** `performMigration()` encounters a non-absent error reading `deviceInfo` from PersistentStore (e.g., storage interface unavailable)
- **THEN** `performMigration()` aborts and returns an error to prevent data loss
- **THEN** filesystem migration import is not attempted

### Requirement: Rollback synchronization SHALL run after successful persistence writes
Mutating persistence paths SHALL invoke AS-file synchronization only after successful PersistentStore writes when migration support is enabled.

#### Scenario: Successful persistence triggers rollback synchronization
- **WHEN** a mutating operation persists deviceInfo successfully
- **THEN** rollback synchronization to the filesystem source is invoked in the success path

#### Scenario: Filesystem sync failure after persistence success
- **WHEN** PersistentStore write succeeds and filesystem sync fails
- **THEN** failure diagnostics are emitted
- **THEN** PersistentStore success remains authoritative and is not rolled back

### Requirement: Compile-time migration gating SHALL use canonical flag naming
Migration import and rollback synchronization code paths SHALL be controlled by `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` at compile time.

#### Scenario: Migration gating enabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` enabled
- **THEN** migration and rollback synchronization paths are compiled and available

#### Scenario: Migration gating disabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` disabled
- **THEN** migration and rollback synchronization ticket paths are excluded from compilation

### Requirement: Baseline persistence behavior SHALL remain functional with migration disabled
Disabling migration support SHALL NOT break non-migration Bluetooth persistence behavior.

#### Scenario: Baseline behavior with migration disabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` disabled and runtime mutations occur
- **THEN** baseline Bluetooth persistence paths continue to function without migration dependencies

### Requirement: Power-state-driven connection management SHALL be compile-time gated
Power-state-driven connection management on transitions to and from `POWER_STATE_ON`, `POWER_STATE_STANDBY`, `POWER_STATE_STANDBY_LIGHT_SLEEP`, and `POWER_STATE_STANDBY_DEEP_SLEEP` SHALL be compiled and active only when `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` is enabled.

#### Scenario: Power state transitions active with migration enabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` enabled
- **THEN** power-state-driven connection management executes on relevant transitions (e.g., Bluetooth auto-enabled on wake when paired non-HID devices exist; non-HID devices disconnected on standby or deep sleep based on their autoconnect setting)

#### Scenario: Power state transitions inactive with migration disabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` disabled
- **THEN** `onPowerModeChanged` returns immediately and no power-state-driven connection or disconnection occurs

### Requirement: Startup SHALL disconnect externally connected devices without explicit autoconnect
During initialization, the plugin SHALL disconnect any currently connected non-HID device that does not have autoconnect explicitly set, to ensure only intentionally autoconnected devices remain connected after plugin activation.

#### Scenario: Externally connected non-HID device without explicit autoconnect
- **WHEN** initialization finds a connected non-HID device whose autoconnect flag has not been explicitly configured
- **THEN** the plugin disconnects the device

#### Scenario: Connected HID device at startup
- **WHEN** initialization finds a connected Human Interface Device
- **THEN** the device is left connected regardless of autoconnect setting

### Requirement: External connect requests SHALL be auto-handled when autoconnect is explicitly set
When an external Bluetooth device requests a connection and autoconnect has been explicitly configured for that device, the plugin SHALL resolve the request internally without propagating the event to clients.

#### Scenario: Autoconnect explicitly enabled
- **WHEN** an external connect request arrives for a device with autoconnect explicitly set to enabled
- **THEN** the plugin accepts the connection, initiates device connection, and does not emit an `onConnectionRequest` event

#### Scenario: Autoconnect explicitly disabled
- **WHEN** an external connect request arrives for a device with autoconnect explicitly set to disabled
- **THEN** the plugin rejects the connection without emitting an `onConnectionRequest` event

#### Scenario: Autoconnect not explicitly set
- **WHEN** an external connect request arrives for a device with no explicit autoconnect setting
- **THEN** the plugin emits an `onConnectionRequest` event to clients to allow them to decide
