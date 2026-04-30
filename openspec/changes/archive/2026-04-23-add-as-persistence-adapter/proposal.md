## Why

Bluetooth migration and rollback behavior for CPESP-9452 requires a dedicated adapter for AS-file persistence that is isolated from BluetoothDeviceManager business logic. Creating this adapter now reduces risk in AC1 and AC2 paths by centralizing file I/O, schema mapping, and parse/write failure handling.

## What Changes

- Add a new Bluetooth AS persistence adapter that reads and writes /opt/persistent/sky/sky-asperipherals-bluetoothdevices.json.
- Define adapter responsibilities for schema-aware parse/serialize behavior, tolerant fallback on malformed input, and atomic write semantics.
- Integrate adapter use points in BluetoothDeviceManager behind BLUETOOTH_ENABLE_AS_MIGRATION so AC1/AC2 paths can be enabled or excluded at compile time.
- Add focused L1 coverage for migration parse fallback edge cases and adapter-driven behavior verification.

## Capabilities

### New Capabilities
- `bluetooth-as-persistence-adapter`: Introduces a dedicated adapter layer for AS-file read/parse/write mapping used by CPESP-9452 migration and rollback flows.

### Modified Capabilities
- None.

## Impact

- Affected code: Bluetooth adapter and manager implementation, Bluetooth build wiring, and L1 Bluetooth tests.
- External contract impact: AS file schema consumption and serialization aligned to docs/paired_bluetooth_devices.schema.json.
- Runtime behavior: AC1 migration and AC2 rollback paths become cleaner and easier to validate while remaining compile-time gated.
