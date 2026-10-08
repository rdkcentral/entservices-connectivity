## Why

The current Bluetooth migration logic auto-triggers during `init()` when the PersistentStore namespace is absent. This implicit behavior is insufficient for a deployment model where the client (IUI) must control when migration occurs and must be able to roll back by clearing migrated state. Two new APIs are required to make migration explicitly client-driven, and to introduce a checksum-based sync model so that re-triggering `performMigration` is safe and idempotent.

## What Changes

- Remove auto-migration from `init()`.
- Add `performMigration` API: imports AS filesystem persistence into RDK PersistentStore on first call; on subsequent calls, re-syncs only if the AS file has changed (checksum comparison).
- Add `clearMigration` API: wipes the RDK PersistentStore namespace, clearing both `deviceInfo` and `fsChecksumAtLastSync`.
- Add a pre-migration guard on `setAutoConnect`: rejected when migration has not yet been performed.
- Introduce `fsChecksumAtLastSync` as a new PersistentStore key under the `Bluetooth` namespace, doubling as a migration-state sentinel.
- Keep `getAutoConnect` behavior unchanged (returns existing cache state regardless of migration state).

## Capabilities

### New Capabilities
- `bluetooth-client-triggered-migration`: Requirements for `performMigration`, `clearMigration`, checksum-based sync, pre-migration guard on `setAutoConnect`, and the `fsChecksumAtLastSync` sentinel key.

### Modified Capabilities
- `bluetooth-migration`: Replace the auto-migration-at-init requirement with the client-triggered model.

## Impact

- `BluetoothDeviceManager` (`.h` and `.cpp`): new public methods, new PS key constant, new `_isMigrated` state, pre-migration guard on `setAutoConnect`.
- `Bluetooth.cpp` / `Bluetooth.h`: register two new JSON-RPC methods, new wrapper implementations.
- Callers of `setAutoConnect` in pre-migration state will now receive a failure response until `performMigration` has been called at least once.
- No change to `getAutoConnect`, `getPairedDevices`, `getConnectedDevices`, or any other existing API behavior.
