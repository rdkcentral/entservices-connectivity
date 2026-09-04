## ADDED Requirements

### Requirement: Common plugin has no direct SDK dependency
The common Bluetooth plugin SHALL load on products without the real Bluetooth SDK and SHALL have no `DT_NEEDED` entry for `librdk_bluetooth.so`.

#### Scenario: Legacy product loads plugin
- **WHEN** the real SDK marker is absent
- **THEN** the Bluetooth plugin SHALL load without resolving `librdk_bluetooth.so`

### Requirement: SDK backend is loaded on demand
The SDK-backed implementation SHALL reside in a private module that is loaded only after the real SDK marker is detected.

#### Scenario: SDK marker is absent
- **WHEN** `/usr/lib/bluetoothsdk/librdk_bluetooth.so` is absent
- **THEN** the plugin SHALL not load the real SDK or SDK backend module and SHALL use BTMgr

#### Scenario: SDK marker is present
- **WHEN** the real SDK marker is present
- **THEN** the plugin SHALL load the real SDK before the SDK backend module and SHALL use the SDK implementation

### Requirement: SDK load failure is controlled
The plugin SHALL fail initialization when a positive SDK marker check is followed by an SDK, backend-module, factory, or SDK-adapter initialization failure. It SHALL NOT fall back to BTMgr in that case.

### Requirement: Factory ownership is explicit
The SDK backend module SHALL export C functions to create and destroy its `IBtAdapter` instance. The main plugin SHALL destroy SDK adapters through the module's destroy function.

### Requirement: Dynamic modules remain loaded
After plugin deinitialization, the plugin SHALL deinitialize and destroy the SDK adapter but SHALL retain the SDK and backend-module handles until process exit.
