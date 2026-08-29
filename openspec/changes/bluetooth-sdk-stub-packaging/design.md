## Context

`runtime-backend-selection` decided that the plugin compiles both backends and chooses one at initialization by testing for `/usr/lib/bluetoothsdk/librdk_bluetooth.so`. That change addressed the runtime half of the problem and left the build half implicit.

The build half turns out to be the binding constraint. Middleware ships as a prebuilt IPK common to all products, so the plugin binary is fixed before anyone knows whether a given device has the SDK. Three separate couplings to the SDK exist today, and only the first was visible in the earlier design:

1. `find_package(BLUETOOTH_SDK QUIET)` in `Bluetooth/CMakeLists.txt` gates which sources compile, so binary contents vary with the build sysroot.
2. `target_link_libraries(... ${BLUETOOTH_SDK_LIBRARIES})` records a `DT_NEEDED` entry. On a product without the SDK the plugin fails to load entirely, so the runtime check never executes.
3. `BtSdkAdapterImpl.h` includes `bluetooth/Manager.h`, which includes `bluetooth/sdbus/*`, which requires sdbus-c++ headers. Compiling the SDK path needs sdbus-c++ in the sysroot, not just the SDK.

The MotionDetector component solves the equivalent problem with a HAL stub. `MotionDetection` links `md-hal` unconditionally with no fallback path; `virtual/vendor-motiondetector-hal` is satisfied by the real vendor HAL or by `motiondetector-hal-noop`; both install `libmd-hal.so` with an explicit `-Wl,-soname,libmd-hal.so`; `PREFERRED_PROVIDER` selects per product. `HAS_MOTION_DETECTOR` only controls whether the vendor packagegroup depends on the HAL — it does not gate the middleware compile.

The one place Bluetooth cannot follow MotionDetector exactly is behavioral. MotionDetector has no alternative implementation: on a stub image the calls are no-ops and the feature is simply absent, which is acceptable for a device with no motion sensor. Bluetooth must keep working on non-SDK products via BTMgr, so the plugin has to distinguish a real SDK from a stub rather than calling whatever is behind the SONAME.

SDK rollout is staggered and outside this team's control, but every product is expected to converge on the SDK. This design is therefore explicitly temporary and optimized for removability.

## Goals / Non-Goals

**Goals:**
- One middleware binary that links and loads on every product, with or without the SDK.
- Zero build dependency on the *real* SDK in the middleware stream.
- Preserve the runtime selection rule and the truthfulness of the path it tests.
- Keep `btmgr.service`'s existing start condition working unmodified.
- Follow the MotionDetector provider convention wherever it applies.
- Be deletable in a small, obvious changeset when rollout completes.

**Non-Goals:**
- A functional stub. It is never called.
- Changing the runtime selection rule.
- Modifying SDK public headers.
- Restructuring the backends into separately packaged provider libraries.

## Decisions

### 1. Stub the SDK rather than avoid linking it

The stub satisfies the plugin's undefined SDK symbols so the middleware build succeeds and the resulting binary loads everywhere.

**Reasoning:**
- Directly matches the MotionDetector pattern already proven in the field.
- Requires no change to plugin source; the runtime selection logic already written stays as-is.
- Failure mode is loud: if the plugin starts referencing an SDK symbol the stub does not define, the middleware link fails at build time rather than misbehaving in the field.

**Alternatives considered:**
- *Weak symbols / `--allow-shlib-undefined` plus `dlopen`*: rejected. C++ vtables and constructors resolved through `dlopen` are error-prone, and undefined-symbol relaxation turns genuine link errors into runtime crashes.
- *Split each backend into its own provider library behind `IBtAdapter`*: architecturally cleaner and eliminates the stub, sdbus-c++, and SONAME concerns entirely — but relocates `BtSdkAdapterImpl`, `EventBridge`, `AuthBridge`, and `DeviceTypeClassifier` plus their tests into a separate repository and introduces a published ABI contract between independently built artifacts. Set aside as disproportionate for a temporary bridge, and recorded here as the fallback if the stub's maintenance cost proves worse than expected.
- *A BTMgr-backed shim wearing the SDK's API*: rejected. It would require reimplementing roughly 6,300 lines of a C++ API owned by another team, including a class whose base constructor stands up a D-Bus proxy to `org.bluez`.

### 2. Both providers expose the SDK library in the default library directory; the real provider owns the SDK marker path

The stub installs `librdk_bluetooth.so` to `${libdir}`. The real SDK remains installed at `${libdir}/bluetoothsdk/librdk_bluetooth.so` and is exposed at `${libdir}/librdk_bluetooth.so` through a symlink. The stub never installs the SDK-directory marker.

The real-provider metadata and `${libdir}` symlink are supplied by a `bluetoothsdk_%.bbappend` in the product layer, not by modifying the SDK-owned recipe or repository. The real SDK's existing `${libdir}/bluetoothsdk/librdk_bluetooth.so` installation remains the marker path; the overlay's `${libdir}/librdk_bluetooth.so` symlink points to it.

**Reasoning:**
- The dynamic loader searches `${libdir}` by default, but neither Yocto's normal configuration nor this repository provides a search path for `${libdir}/bluetoothsdk/`; `USE_LDCONFIG = "0"` also rules out a dependable `ld.so.conf.d` solution. Installing the selected provider at `${libdir}` is therefore mandatory.
- The virtual provider selection guarantees only one provider exposes `${libdir}/librdk_bluetooth.so` on an image. The real provider's symlink preserves the existing marker contract without affecting resolution.
- If the stub installed the SDK marker, two things would break: the plugin's existence check would be true everywhere, and `btmgr.service`'s `ConditionPathExistsGlob=!/usr/lib*/bluetoothsdk/librdk_bluetooth.so` would skip btmgr on precisely the products that depend on it.
- Keeping the real path exclusive to the real SDK preserves a single source of truth that both the plugin and the systemd unit already consume.

**Alternatives considered:**
- *Putting the real library only in `${libdir}/bluetoothsdk/` and adding an RPATH or loader configuration*: rejected. The repository has no RPATH convention and Yocto normally removes embedded RPATHs; `USE_LDCONFIG = "0"` prevents relying on an `ld.so.conf.d` workaround.
- *Same marker path for both, with a stub-only marker symbol discovered via `dlsym(RTLD_DEFAULT, ...)`*: workable and fully under this team's control, since the stub can export symbols the real SDK never will. Rejected because it does not fix the `btmgr.service` condition. Remains available as a secondary assertion.
- *A marker file installed by the real SDK recipe*: requires a change to a recipe owned by another team, and adds a second source of truth.

### 3. Both backends always compile; the SDK always links

`Bluetooth/CMakeLists.txt` drops `find_package(BLUETOOTH_SDK QUIET)`-driven source selection, the `BTMGR_FOUND` branch, and the `FATAL_ERROR` fallback for the production path. `BLUETOOTH_HAS_SDK` and `BLUETOOTH_HAS_BTMGR` are unconditionally defined for production builds.

**Reasoning:**
- With a provider guaranteed present, configuration-time branching serves no purpose and is the mechanism that made the binary vary by sysroot.
- The `BLUETOOTH_TEST_BACKEND` path for L1/L2 builds is unaffected and stays as-is; test builds link neither the real SDK nor the stub.

### 3a. The shared middleware IPK owns no Bluetooth-stack runtime dependencies

`entservices-connectivity` retains build dependencies on BTMgr and `virtual/bluetooth-sdk` because it compiles both implementations, but it SHALL NOT runtime-depend on either `bluetooth-mgr` or `bluetoothsdk`. The vendor layer owns image composition and is responsible for installing a consistent Bluetooth stack. It may keep `bluetooth-mgr` installed but inactive on SDK images; the existing systemd marker condition controls service activation.

**Reasoning:**
- The middleware IPK is prebuilt and shared across products, so hard runtime dependencies would wrongly force one stack onto every image.
- Package composition is vendor-specific and already belongs to the vendor-layer team.
- A missing runtime stack must be diagnosed at plugin initialization, where the existing backend logic provides the failure path; middleware packaging must not try to select or install it.

### 4. Static initialization must not touch the SDK

The production default instances are constructed at load time:

```cpp
static BtSdkAdapterImpl g_btSdkAdapterImpl;
```

This is safe only because every SDK-typed member of `BtSdkAdapterImpl` is held behind a smart pointer (`std::unique_ptr<bluetooth::Manager>`, `std::shared_ptr<bluetooth::Adapter>`, the `EventBridge` and `AuthBridge` pointers), so constructing it touches no SDK symbol. Nothing from `librdk_bluetooth.so` executes until `init()`, which is never called on a stub image.

This is a load-bearing invariant, not an incidental property. Adding an SDK-typed member by value, or SDK work in the constructor, would cause stub code to execute at plugin load on every non-SDK product.

### 5. Matching SONAMEs are mandatory

Both the real library and the stub must be linked with `-Wl,-soname,librdk_bluetooth.so`.

**Reasoning:**
- `FindBLUETOOTH_SDK.cmake` locates the library with `find_library`, handing the linker an absolute sysroot path. The linker records that path in `DT_NEEDED` unless the library declares a SONAME, in which case the SONAME is recorded instead. Only the latter is swappable.
- `bluetooth-sdk/src/CMakeLists.txt` declares no `VERSION` or `SOVERSION`, but CMake's `NO_SONAME` property defaults to off, so a `SHARED` target on an ELF platform is expected to carry `SONAME=librdk_bluetooth.so` regardless. The missing properties mean no versioned symlinks, not no SONAME.
- This is therefore expected to hold already and needs confirmation rather than a fix. `motiondetector-hal-stubs` passes `-Wl,-soname` explicitly only because it is a raw Makefile; the stub here must do the same.
- If the expectation turns out to be wrong, the fix lands in a recipe this team does not own, which is why it is verified first.

### 6. The stub defines only what the plugin references

The stub is not a reimplementation of the SDK's ~6,300-line header surface. It defines the out-of-line symbols the plugin's link actually requires, compiled against copies of the SDK headers so that mangled names match.

**Reasoning:**
- Keeps the stub small and its update cadence tied to the plugin's own SDK usage.
- Drift surfaces as a middleware link error, which is a tractable failure mode. This also means the symbol set does not have to be enumerated authoritatively up front: it can be bootstrapped from source analysis and completed from link errors.
- Source analysis of `BtSdkAdapterImpl`, `EventBridge`, `AuthBridge`, and `DeviceTypeClassifier` shows 18 distinct SDK methods in use across `Manager`, `Adapter`, and `Device`, implying roughly 25-35 out-of-line symbols once constructors, destructors, vtables, and typeinfo are counted. The `GattServer`, `GattClient`, and generated sdbus proxy headers are pulled in transitively but nothing in them needs defining.

**Consequence:** because `bluetooth::Manager` derives from `sdbus::ProxyInterfaces<...>`, defining its constructor emits base-class references plus vtable and typeinfo. The stub therefore links sdbus-c++, and `libsdbus-c++` becomes a runtime dependency on every image including non-SDK products. This is unavoidable without refactoring headers owned by another team.

### 7. The SDK-backed sources must first be made to compile against the real headers

A static audit of `Tests/mocks/bluetooth/*.h` against `bluetooth-sdk/include/**` found that the plugin's SDK-backed sources have only ever been compiled against the test mocks, which describe a different API than the SDK actually exposes. The stub cannot be built until this is closed, because a stub compiled from the real headers will not let the plugin compile, and a stub compiled from the mock headers would not be ABI-compatible with the real library.

The audit found the divergence is narrow. Only one item blocks:

- `bluetooth::Adapter` privately inherits `sdbus::ProxyInterfaces<org::bluez::Adapter1_proxy, ...>`, so `Alias()`, `Discoverable()`, and `DiscoverableTimeout()` are inaccessible from outside the class. The mock declares them as public pure virtuals. Five call sites in `BtSdkAdapterImpl.cpp` depend on them, backing four `IBtAdapter` operations: `getAdapterName`, `setAdapterName`, `isAdapterDiscoverable`, `setAdapterDiscoverable`.

Everything else compiles against the real headers: `Device::getAllProperties` returns `Status` rather than `bool`, but `Status` defines `operator bool()`; `DeviceState` orders its enumerators differently but the three the plugin uses exist in both; `registerForEvents`/`unregisterForEvents` resolve through the real `EventEmitter<>` template base; `Manager`, `DeviceProperties`, and `ScanFilter` match.

Note also that the mocks cannot simply be replaced with copies of the real headers. Real `Adapter` and `Device` are concrete classes whose relevant methods are non-virtual, so they cannot be mocked by inheritance. The parallel pure-virtual mock is a deliberate testability choice and should remain one.

**Reasoning:**
- The stub's whole purpose is ABI compatibility with the real library. That is meaningless if the consumer cannot compile against the real API.
- Closing a four-operation gap is far cheaper than discovering it during middleware integration.

### 8. The real SDK public API is the mock contract

The test mocks SHALL model only the public API exposed by the headers in `bluetooth-sdk/include`. They SHALL NOT expose proxy methods merely because those methods happen to exist on a private implementation base class.

**Reasoning:**
- The SDK public headers are the only supported contract available to `entservices-connectivity`; the private `sdbus::ProxyInterfaces` inheritance is deliberately not part of that contract.
- The current mock-first workflow allowed `BtSdkAdapterImpl` to compile while calling inaccessible implementation details. Aligning the mock first makes the actual SDK API the source of truth and turns future divergence into a compile-time signal.
- The real `Adapter` and `Device` classes cannot be substituted directly in gmock tests because they are concrete and their relevant operations are non-virtual. The existing parallel pure-virtual mock remains appropriate, but its method set and signatures must be kept aligned with the real public API.

**Implementation consequence:** first refactor `BtSdkAdapterImpl` so it compiles against the actual public SDK headers. The four adapter-name and discoverability operations must either use new public SDK accessors supplied by the SDK team, or return an explicit unsupported failure on the SDK backend until those accessors are available. Then reduce the mocks to the matching public interface and align signature/value mismatches such as `Device::getAllProperties` returning `Status` and `DeviceState` enumerator ordering.

## Risks / Trade-offs

- [Stub drifts behind the plugin's SDK usage] → Mitigation: drift manifests as a middleware link failure, not a field defect; treat stub updates as part of any change that touches `BtSdkAdapterImpl`.
- [Copied SDK headers go stale] → Mitigation: pin the copies to a known `bluetooth-sdk` revision and record it; consider a CI diff against that revision.
- [`libsdbus-c++` footprint on flash-constrained products] → Mitigation: quantify before committing; it is a library only, with no daemon or service.
- [Missing SONAME defeats the whole mechanism] → Mitigation: expected to be satisfied by CMake's default behavior for `SHARED` ELF targets. No build host or SDK-enabled image is available to this team, so verification is deferred to the first middleware integration build; if the expectation fails it costs one integration cycle and a one-line recipe fix.
- [`libsdbus-c++` footprint turns out to be unacceptable] → Mitigation: deferred to the same integration build. If it is prohibitive, the fallback is the provider-library split described under Decision 1, which removes the dependency from non-SDK images entirely.
- [A future change adds an SDK-typed member by value or SDK work in a constructor] → Mitigation: record the static-initialization invariant in `BtSdkAdapterImpl.h` and cover it in review.
- [An image installs the stub while also lacking btmgr, or installs the real SDK while btmgr still runs] → Mitigation: have each provider express its runtime dependency on the corresponding stack so a self-inconsistent image fails at compose time rather than at boot.
- [The temporary bridge outlives its purpose] → Mitigation: the removal path is small and documented; revisit once the last product ships the SDK.
- [Four adapter operations have no real SDK API behind them] → Mitigation: see Decision 7. Either the SDK exposes the three BlueZ adapter properties publicly, or those operations are degraded on the SDK backend until it does. Blocks the stub work until resolved.
- [Mock and real SDK APIs drift again] → Mitigation: the mocks are an intentional parallel interface and cannot be generated from the real headers, so drift is only detectable by audit or by a production compile. Treat "does this compile against the real SDK" as a standing review question for any change to the SDK-backed sources.
- [Mocks expose private SDK implementation details] → Mitigation: Decision 8 establishes the public SDK headers as the sole mock contract; remove any mock-only API before stub work begins.

## Migration Plan

1. Verify the SONAME situation on a current SDK-enabled build. If absent, fix it in the SDK recipe before proceeding.
2. Determine the exact set of SDK symbols the plugin's production link requires.
3. Create the stub repository and recipe, providing `virtual/bluetooth-sdk` at middleware architecture.
4. Add `PROVIDES`/`RPROVIDES` to the real `bluetoothsdk` recipe.
5. Set the middleware default `PREFERRED_PROVIDER` to the stub and add per-product overrides for SDK-enabled products.
6. Simplify `Bluetooth/CMakeLists.txt` to always compile both backends and always link the SDK.
7. Validate on both an SDK-enabled and a non-SDK image: the plugin loads, selects the expected backend, and `btmgr.service` starts or is skipped correctly.

## Open Questions

- How should the four blocked adapter operations be resolved: an SDK API addition exposing the BlueZ adapter properties, or degradation on the SDK backend until one lands?
- Which revision of `bluetooth-sdk/include` is the compatibility baseline for the aligned mocks and the stub?
- Does the real `librdk_bluetooth.so` carry a SONAME as expected, and does the plugin's `DT_NEEDED` entry record it rather than a sysroot path?
- Who owns the change to the real `bluetoothsdk` recipe for `PROVIDES` and, if needed, the SONAME?
- Is the added `libsdbus-c++` footprint acceptable on the most constrained products?
- Which repository should host the stub, and does it follow the `motiondetector-hal-stubs` naming and layer conventions?
- Should the stub also export a marker symbol as a secondary assertion that the loader resolved the expected library?
