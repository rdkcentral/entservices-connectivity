## ADDED Requirements

### Requirement: Runtime backend selection
The Bluetooth plugin SHALL determine its active backend exactly once during initialization by checking whether `/usr/lib/bluetoothsdk/librdk_bluetooth.so` exists on the filesystem.

#### Scenario: SDK library is present
- **WHEN** the plugin initializes and `/usr/lib/bluetoothsdk/librdk_bluetooth.so` exists
- **THEN** the plugin SHALL select the bluetooth-sdk implementation as the active backend for the lifetime of that plugin instance

#### Scenario: SDK library is missing
- **WHEN** the plugin initializes and `/usr/lib/bluetoothsdk/librdk_bluetooth.so` does not exist
- **THEN** the plugin SHALL select the BTMgr-based implementation as the active backend for the lifetime of that plugin instance

### Requirement: Backend choice is stable
The plugin SHALL keep the selected backend for the lifetime of the plugin instance once initialization has completed.

#### Scenario: Initialization completes
- **WHEN** the backend has been selected during initialization
- **THEN** the plugin SHALL continue using that backend and SHALL NOT re-evaluate the filesystem check during normal operation

### Requirement: Adapter abstraction remains stable
The Bluetooth plugin SHALL preserve the existing `IBtAdapter` contract and forwarding behavior while selecting a concrete implementation at runtime.

#### Scenario: Producing a runtime-selected adapter instance
- **WHEN** the plugin creates its default adapter instance
- **THEN** the adapter SHALL route calls through the selected backend implementation without changing the external plugin API
