## 0. Deferred verification

No build host or SDK-enabled image is available to this team, so these are deliberately deferred to the first middleware integration build rather than gating the work. Both fail loudly and cheaply if the assumption is wrong.

- [ ] 0.1 At first integration build, confirm `librdk_bluetooth.so` declares `SONAME librdk_bluetooth.so` as CMake's defaults imply
- [ ] 0.2 At first integration build, confirm the plugin's `NEEDED` entry for the SDK is that SONAME and not an absolute sysroot path
- [ ] 0.3 If either expectation fails, raise a one-line SONAME fix with the owner of the real `bluetoothsdk` recipe
- [ ] 0.4 At first integration build, measure the `libsdbus-c++` footprint and confirm it is acceptable on the most flash-constrained product

## 1. Close the mock/SDK API divergence

Prerequisite for all stub work. See Decision 7.

- [x] 1.0 Audit `Tests/mocks/bluetooth/*.h` against the real SDK headers and enumerate every divergence
- [x] 1.1 Pin the real `bluetooth-sdk/include` revision that defines the mock and stub compatibility baseline (`31008a87958a89fb43bbc6ba06ad5312dbda3e77`)
- [x] 1.2 Refactor `BtSdkAdapterImpl` to use only the SDK's public `Adapter`, `Device`, and `Manager` APIs
- [x] 1.3 Resolve the four blocked adapter operations (`getAdapterName`, `setAdapterName`, `isAdapterDiscoverable`, `setAdapterDiscoverable`) by reporting them unsupported on the SDK backend until an SDK API addition lands
- [x] 1.4 Remove mock-only API and align all mock signatures and enum values with the pinned public SDK headers, including `DeviceState` ordering and `Device::getAllProperties` returning `Status`
- [ ] 1.5 Confirm the SDK-backed sources compile against the real headers once the above land

## 2. Stub definition

- [x] 2.1 Bootstrap the symbol set from source analysis of the plugin's SDK-backed sources, then complete it iteratively from middleware link errors
- [x] 2.2 Create the stub repository following `motiondetector-hal-stubs` conventions, pinned to a recorded `bluetooth-sdk` revision
- [x] 2.3 Copy the SDK headers required to compile the stub and record the source revision alongside them
- [x] 2.4 Implement no-op definitions for the required symbols
- [x] 2.5 Link the stub with `-Wl,-soname,librdk_bluetooth.so`
- [x] 2.6 Do not export a stub-only marker symbol; the real SDK install path remains the single runtime discriminator

## 3. Recipes and provider selection

- [x] 3.1 Add the stub recipe providing `virtual/bluetooth-sdk` at middleware architecture
- [x] 3.2 Install the stub to the default library directory, not `${libdir}/bluetoothsdk/`
- [x] 3.3 Add a product-layer `bluetoothsdk_%.bbappend` that supplies `PROVIDES`/`RPROVIDES` for `virtual/bluetooth-sdk` and a `${libdir}/librdk_bluetooth.so` symlink to the real SDK marker, without modifying the SDK-owned recipe
- [x] 3.4 Set the middleware default `PREFERRED_PROVIDER_virtual/bluetooth-sdk` to the stub
- [x] 3.5 Add per-product overrides selecting the real SDK on SDK-enabled products
- [x] 3.6 Keep `entservices-connectivity` free of `bluetooth-mgr` and `bluetoothsdk` runtime dependencies; vendor-layer packaging owns installation and service activation of the selected stack
- [x] 3.7 Use the default dynamic-loader path for both providers; the real SDK's `${libdir}/bluetoothsdk/` entry is a marker symlink only, not a loader search path

## 4. Plugin build configuration

- [x] 4.1 Remove `find_package`-driven backend source selection and the `FATAL_ERROR` fallback from the production path in `Bluetooth/CMakeLists.txt`
- [x] 4.2 Always compile both backend implementations and always link the SDK for production builds
- [x] 4.3 Define `BLUETOOTH_HAS_SDK` and `BLUETOOTH_HAS_BTMGR` unconditionally for production builds
- [ ] 4.4 Leave the `BLUETOOTH_TEST_BACKEND` path for L1/L2 builds unchanged and confirm test builds link neither the real SDK nor the stub
- [x] 4.5 Confirm `Bluetooth/BtAdapter.cpp` requires no change

## 5. Invariants

- [x] 5.1 Record the static-initialization invariant in `BtSdkAdapterImpl.h`: SDK-typed members stay behind smart pointers and the constructor performs no SDK work
- [x] 5.2 Leave `btmgr-bluetooth-sdk.conf` unmodified and note that its correctness depends on the stub's install location

## 6. Validation

- [ ] 6.1 Build middleware in a sysroot providing only the stub and confirm both backends compile and the plugin links
- [ ] 6.2 Confirm the plugin's recorded SDK dependency is the SONAME, not a build-tree path
- [ ] 6.3 On a non-SDK image: plugin loads, BTMgr backend is selected, `btmgr.service` starts, and Bluetooth is functional
- [ ] 6.4 On an SDK image: plugin loads, SDK backend is selected, `btmgr.service` is skipped, and Bluetooth is functional
- [ ] 6.5 Confirm no stub symbol executes on a stub-only image
- [ ] 6.6 Verify a deliberately missing stub symbol fails the middleware link
- [ ] 6.7 Run the existing L1/L2 Bluetooth suites for both test backends

## 7. Removal path

- [ ] 7.1 Record the steps to retire the bridge once every product ships the SDK: flip provider defaults, drop the stub recipe and repository, remove the BTMgr backend and the runtime check
