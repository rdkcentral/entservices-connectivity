## ADDED Requirements

### Requirement: Single middleware artifact across SDK and non-SDK products
The Bluetooth plugin SHALL be built once as part of the shared middleware artifact and SHALL load successfully on products with and without the RDK Bluetooth SDK installed, without being rebuilt per product.

#### Scenario: Plugin loads on a product without the SDK
- **WHEN** the middleware artifact is installed on a product that does not have the real Bluetooth SDK
- **THEN** all dynamic library dependencies of the plugin SHALL resolve and the plugin SHALL load successfully

#### Scenario: Plugin loads on a product with the SDK
- **WHEN** the same middleware artifact is installed on a product that has the real Bluetooth SDK
- **THEN** all dynamic library dependencies of the plugin SHALL resolve and the plugin SHALL load successfully

### Requirement: Bluetooth SDK virtual provider
The build SHALL define a `virtual/bluetooth-sdk` provider satisfied either by the real Bluetooth SDK or by a no-op stub, selected per product using `PREFERRED_PROVIDER`.

#### Scenario: Middleware default provider
- **WHEN** a middleware image is composed without a product-specific override
- **THEN** the stub provider SHALL be selected

#### Scenario: Product override to the real SDK
- **WHEN** a product declares the real Bluetooth SDK as the preferred provider
- **THEN** the real SDK SHALL be installed and the stub SHALL NOT be installed

### Requirement: Provider install locations preserve the SDK path as single source of truth
The stub provider SHALL install `librdk_bluetooth.so` to the default library directory. The real provider SHALL retain its `${libdir}/bluetoothsdk/librdk_bluetooth.so` installation and expose it through a `${libdir}/librdk_bluetooth.so` symlink. The stub SHALL NOT be installed under `${libdir}/bluetoothsdk/`.

#### Scenario: Loader resolution with the real SDK present
- **WHEN** the plugin is loaded on a product where the real SDK is installed
- **THEN** the loader SHALL resolve `librdk_bluetooth.so` to the real provider's library in the default library directory

#### Scenario: Loader resolution with only the stub present
- **WHEN** the plugin is loaded on a product where only the stub is installed
- **THEN** the loader SHALL resolve `librdk_bluetooth.so` to the stub in the default library directory

#### Scenario: Runtime selection check remains truthful
- **WHEN** only the stub is installed
- **THEN** `/usr/lib/bluetoothsdk/librdk_bluetooth.so` SHALL NOT exist, and the plugin's runtime backend check SHALL select the BTMgr implementation

#### Scenario: Legacy Bluetooth Manager start condition is unaffected
- **WHEN** only the stub is installed
- **THEN** the `btmgr.service` start condition on `/usr/lib*/bluetoothsdk/librdk_bluetooth.so` SHALL evaluate such that btmgr starts normally

### Requirement: Matching SONAME across providers
The real Bluetooth SDK library and the stub SHALL both declare the SONAME `librdk_bluetooth.so`, and the plugin's recorded dynamic dependency SHALL reference that SONAME rather than an absolute build-time path.

#### Scenario: Recorded dependency is swappable
- **WHEN** the plugin binary is inspected after a middleware build
- **THEN** its dynamic dependency entry for the SDK SHALL be the SONAME `librdk_bluetooth.so`

### Requirement: Both backends are always built into the production binary
The production build SHALL compile both the SDK-backed and BTMgr-backed adapter implementations unconditionally and SHALL NOT vary the compiled source set based on whether the real SDK was discovered during configuration.

#### Scenario: Middleware build without the real SDK
- **WHEN** the plugin is built in a sysroot providing only the stub
- **THEN** both backend implementations SHALL be compiled into the plugin and the SDK library SHALL be linked

### Requirement: Stub code is never executed
The stub SHALL consist solely of no-op definitions, and no stub symbol SHALL be executed on any product.

#### Scenario: Plugin load on a stub-only product
- **WHEN** the plugin is loaded on a product where only the stub is installed
- **THEN** no stub symbol SHALL be invoked during static initialization or during plugin initialization

#### Scenario: Adapter construction does not touch the SDK
- **WHEN** the SDK-backed adapter instance is constructed at load time
- **THEN** it SHALL NOT construct, call, or otherwise reference any Bluetooth SDK object, and all SDK-typed members SHALL remain unset until backend initialization

### Requirement: Stub completeness is enforced at build time
The stub SHALL define every Bluetooth SDK symbol referenced by the plugin, such that an unsatisfied reference fails the middleware build rather than deferring the failure to runtime.

#### Scenario: Plugin references a new SDK symbol
- **WHEN** the plugin is changed to reference an SDK symbol the stub does not define
- **THEN** the middleware link SHALL fail
