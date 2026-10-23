## 0. Replace the superseded stub approach

- [x] 0.1 Remove the Bluetooth SDK stub repository, stub recipe, virtual-provider selections, and real-SDK recipe overlay
- [x] 0.2 Remove `virtual/bluetooth-sdk` from the middleware recipe; retain its build-time BTMgr dependency only

## 1. Close the mock/SDK API divergence

Prerequisite for all stub work. See Decision 7.

- [x] 1.0 Audit `Tests/mocks/bluetooth/*.h` against the real SDK headers and enumerate every divergence
- [x] 1.1 Pin the real `bluetooth-sdk/include` revision that defines the mock and stub compatibility baseline (`31008a87958a89fb43bbc6ba06ad5312dbda3e77`)
- [x] 1.2 Refactor `BtSdkAdapterImpl` to use only the SDK's public `Adapter`, `Device`, and `Manager` APIs
- [x] 1.3 Resolve the four blocked adapter operations (`getAdapterName`, `setAdapterName`, `isAdapterDiscoverable`, `setAdapterDiscoverable`) by reporting them unsupported on the SDK backend until an SDK API addition lands
- [x] 1.4 Remove mock-only API and align all mock signatures and enum values with the pinned public SDK headers, including `DeviceState` ordering and `Device::getAllProperties` returning `Status`
- [ ] 1.5 Confirm the SDK-backed sources compile against the real headers once the above land

## 2. SDK backend module

- [x] 2.1 Add a C factory ABI that creates and destroys an `IBtAdapter` owned by the SDK module
- [x] 2.2 Build `BtSdkAdapterImpl`, `EventBridge`, `AuthBridge`, and `DeviceTypeClassifier` into `libBluetoothSdkBackend.so`
- [x] 2.3 Keep SDK headers and `librdk_bluetooth.so` dependencies confined to the module target

## 3. Runtime selection and lifecycle

- [x] 3.1 Load the real SDK by its marker path with `RTLD_NOW | RTLD_GLOBAL`, then load the private SDK module and resolve its factory functions
- [x] 3.2 Select BTMgr without loading SDK code when the marker is absent
- [x] 3.3 Treat an SDK/module/factory failure after a positive marker check as a plugin-initialization failure; do not fall back to BTMgr
- [x] 3.4 Call the module destroy function after adapter deinitialization and retain module handles until process exit
- [ ] 3.5 Confirm target `${libdir}` on each supported product; update the SDK-marker check if any target uses `/usr/lib64`

## 4. Plugin build configuration

- [x] 4.1 Link the main plugin only to BTMgr and `dl`; ensure it has no SDK `DT_NEEDED` entry
- [x] 4.2 Keep the `BLUETOOTH_TEST_BACKEND` path and test injection working without dynamic loading

## 5. Validation

- [ ] 5.1 Verify the main plugin has no `DT_NEEDED` entry for `librdk_bluetooth.so`
- [ ] 5.2 Verify marker absent selects BTMgr without attempting either dynamic load
- [ ] 5.3 Verify marker present loads the real SDK then the backend module and selects SDK
- [ ] 5.4 Verify marker present plus failed module/factory/init produces a controlled initialization failure without BTMgr fallback
- [x] 5.5 Run the existing L1/L2 Bluetooth suites for both test backends (all test configurations passed)
