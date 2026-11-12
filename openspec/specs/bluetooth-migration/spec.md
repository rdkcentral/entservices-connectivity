## ADDED Requirements

### Requirement: Initialization SHALL NOT auto-import migration data from the filesystem
Bluetooth initialization SHALL read PersistentStore device info to seed the in-memory cache and SHALL NOT automatically import migration data from the filesystem. Filesystem migration is deferred to an explicit client invocation of `performMigration()`.

#### Scenario: Initialization with existing PersistentStore data
- **WHEN** initialization finds valid Bluetooth/deviceInfo data in PersistentStore
- **THEN** the cache is seeded from PersistentStore
- **THEN** no migration import is attempted

#### Scenario: Initialization with no PersistentStore data
- **WHEN** initialization finds no Bluetooth/deviceInfo data in PersistentStore
- **THEN** the cache is not seeded from PersistentStore
- **THEN** no migration import is attempted: nothing is written to PersistentStore or the AS file, and the AS file is only read, into the in-memory cache, so the startup reconnect can use it (filesystem import into PersistentStore is deferred to an explicit client invocation of `performMigration()`)

### Requirement: `performMigration` SHALL use migrationVersion-based import semantics
The `performMigration()` API SHALL determine whether to import from the filesystem source by checking both the presence and value of the `migrationVersion` key in PersistentStore. If `migrationVersion` is present and its value equals `"1"`, migration is considered complete and no import is performed. If absent, or if the stored value does not equal `"1"`, migration data is imported and `migrationVersion` is set to `"1"` on success.

#### Scenario: First call (no stored migrationVersion)
- **WHEN** `performMigration()` is called and `migrationVersion` is absent from PersistentStore
- **THEN** migration data is imported from the filesystem source into cache and persisted to PersistentStore
- **THEN** `migrationVersion` is written with value `"1"`

#### Scenario: Subsequent call (migrationVersion already present with expected value)
- **WHEN** `performMigration()` is called and `migrationVersion` is present in PersistentStore with value `"1"`
- **THEN** filesystem migration import is skipped
- **THEN** `performMigration()` returns success indicating migration was already complete

#### Scenario: Subsequent call (migrationVersion present with unexpected value)
- **WHEN** `performMigration()` is called and `migrationVersion` is present in PersistentStore but its value does not equal `"1"` (e.g., future version string or corrupted value)
- **THEN** migration is not considered complete
- **THEN** migration data is re-imported from the filesystem source and `migrationVersion` is overwritten with `"1"` on success

#### Scenario: Missing filesystem source
- **WHEN** `performMigration()` is called and the filesystem source file does not exist
- **THEN** the missing file is treated as an empty payload (zero devices)
- **THEN** `performMigration()` proceeds with the empty payload and returns success (`ERROR_NONE`)

#### Scenario: Unreadable or malformed filesystem source
- **WHEN** `performMigration()` is called and the filesystem source file exists but cannot be read (e.g., I/O error) or cannot be parsed (malformed JSON)
- **THEN** `performMigration()` returns an error and aborts migration to prevent data loss

#### Scenario: migrationVersion read fails with unexpected error
- **WHEN** `performMigration()` encounters a non-absent error reading `migrationVersion` from PersistentStore (e.g., storage interface unavailable)
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
Power-state-driven connection management on transitions to and from `POWER_STATE_ON`, `POWER_STATE_STANDBY`, `POWER_STATE_STANDBY_LIGHT_SLEEP`, and `POWER_STATE_STANDBY_DEEP_SLEEP` SHALL be compiled only when `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` is enabled, and SHALL execute only after a successful `performMigration()` (i.e., when migration state is active).

#### Scenario: Power state transitions active with migration enabled
- **WHEN** Bluetooth is built with `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` enabled and migration has been performed (`migrationVersion` present in PersistentStore with value `"1"`)
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

### Requirement: External connect requests SHALL be auto-handled when autoconnect status is known
When `BLUETOOTH_ENABLE_PERSISTENCE_MIGRATION` is enabled and migration has been performed (`migrationVersion` present in PersistentStore with value `"1"`), when an external Bluetooth device requests a connection and its autoconnect status can be determined, the plugin SHALL resolve the request internally without propagating the event to clients. An unset autoconnect status is treated as disabled. Otherwise, the plugin SHALL emit an `onConnectionRequest` event to clients.

#### Scenario: Autoconnect explicitly enabled
- **WHEN** an external connect request arrives for a device with autoconnect explicitly set to enabled
- **THEN** the plugin accepts the connection, initiates device connection, and does not emit an `onConnectionRequest` event

#### Scenario: Autoconnect explicitly disabled
- **WHEN** an external connect request arrives for a device with autoconnect explicitly set to disabled
- **THEN** the plugin rejects the connection without emitting an `onConnectionRequest` event

#### Scenario: Autoconnect not explicitly set
- **WHEN** an external connect request arrives for a device with no explicit autoconnect setting
- **THEN** the plugin treats the device as autoconnect-disabled and rejects the connection without emitting an `onConnectionRequest` event

### Requirement: SDK backend SHALL reconnect the previous audio output device at startup and power-on
With the bluetooth-sdk backend there is no BTMgr daemon to ask about the previously used audio output device after boot, so the plugin SHALL do so itself, after `Initialize()` and when the platform transitions to power ON. Each paired audio output device SHALL be run through the same connect-request decision as an incoming connection (autoconnect status when migrated, otherwise `onConnectionRequest` to the client with a 200 ms wait). A device accepted by the autoconnect status SHALL be connected by the plugin, which retries a failed connect up to three attempts two seconds apart, and the pass SHALL stop after the first device that is connected. A device accepted by the client SHALL NOT be connected by the plugin: the client is expected to follow its accept with its own connect request, and a second connect by the plugin would overlap it. The pass SHALL stop after the first device the client accepts. A device the client rejects SHALL NOT be connected. If the client does not answer, the plugin SHALL retry once after 40 seconds; on that second pass, and only while migration has not been performed, a device that the legacy AS file marks for autoconnect SHALL be connected anyway.

While migration has not been performed, the plugin SHALL load the AS file into the in-memory paired-device cache at `Initialize()`, read-only: nothing is written to PersistentStore or to the AS file. This gives the reconnect each device's last connect time and legacy autoconnect mark. Stale PersistentStore `deviceInfo` remains ignored, and the client-visible autoconnect status remains disabled until migration.

#### Scenario: Migrated, autoconnect enabled
- **WHEN** the platform starts, migration has been performed and a paired audio output device has autoconnect enabled
- **THEN** the plugin connects the device without emitting `onConnectionRequest`

#### Scenario: Not migrated, client accepts
- **WHEN** the platform starts, migration has not been performed, a paired audio output device has a last-connect time in the AS file and the client answers its `onConnectionRequest` with accept
- **THEN** the plugin does not connect the device; connecting it, and recording its last-connect time, is left to the client's own connect request

#### Scenario: Not migrated, client rejects
- **WHEN** the platform starts, migration has not been performed and the client answers `onConnectionRequest` with reject
- **THEN** the plugin does not connect the device, even if the AS file marks it for autoconnect

#### Scenario: Not migrated, AS file marks the device for autoconnect and the client does not answer
- **WHEN** migration has not been performed, the AS file marks a paired audio output device for autoconnect, and the client does not answer `onConnectionRequest` on either the first pass or the second pass 40 seconds later
- **THEN** the client has been sent `onConnectionRequest` on each pass, and on the second pass the plugin connects the device

#### Scenario: Not migrated, device not marked and the client does not answer
- **WHEN** migration has not been performed, the AS file does not mark the device for autoconnect, and the client does not answer on the second pass
- **THEN** the plugin does not connect the device

#### Scenario: Not migrated, AS file loaded at startup
- **WHEN** migration has not been performed and the plugin initializes
- **THEN** the paired-device cache is filled from the AS file for devices the adapter reports as paired, no PersistentStore or AS file write occurs, and a missing, unreadable or malformed AS file, or a failed adapter query, leaves the cache empty without failing initialization

#### Scenario: Client not ready
- **WHEN** the client does not answer within 200 ms
- **THEN** the plugin asks again once after 40 seconds

#### Scenario: Several paired audio output devices
- **WHEN** more than one paired audio output device exists and last-connect times are known
- **THEN** only devices with a known last-connect time are asked about, most recent first, a device the client rejects or that the plugin fails to connect after three attempts (autoconnect status accepted) is skipped, and no further device is asked about once one is connected or accepted by the client

#### Scenario: Accepted device already connected
- **WHEN** the device is already connected when the plugin is about to connect it after the autoconnect status accepted it
- **THEN** the plugin does not connect it again and the pass ends

#### Scenario: Audio output device already connected
- **WHEN** a pass starts and an audio output device is already connected
- **THEN** no device is asked about or connected

#### Scenario: Box not in power state ON
- **WHEN** the platform is in any power state other than ON (STANDBY, STANDBY_LIGHT_SLEEP, STANDBY_DEEP_SLEEP or OFF), whether or not migration has been performed
- **THEN** a reconnect pass does not start, and the plugin reports the power state to the adapter before it applies the power-on reconnect

#### Scenario: Power state leaves ON during a pass
- **WHEN** the platform leaves power state ON while a pass is waiting to ask again after 40 seconds, or between connect attempts
- **THEN** the pass ends without asking again or connecting, and the next transition to ON starts a new pass

#### Scenario: Gamepad connected at startup or connecting later
- **WHEN** a gamepad (not an RDK remote control) is connected at startup or connects, and the connect-request decision rejects it or the client does not answer within 20 seconds
- **THEN** the plugin disconnects it and does not report it to clients; if accepted, an already connected gamepad is reported with `onDeviceFound`

#### Scenario: Plugin-initiated pairing or connection
- **WHEN** the plugin is itself pairing or connecting a device and that device's connection arrives for authorization
- **THEN** the plugin accepts it without consulting autoconnect or the client
