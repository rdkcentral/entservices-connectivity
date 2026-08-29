## Context

The Bluetooth plugin currently resolves its backend during CMake configuration. The production path in `Bluetooth/CMakeLists.txt` calls `find_package(BLUETOOTH_SDK)` and, if it is absent, falls back to `find_package(BTMGR)`. The adapter layer then compiles a single static implementation based on `BLUETOOTH_USE_SDK`, and the rest of the plugin routes all behavior through that fixed instance.

This is workable for a single deployment image, but it ties the runtime backend choice to the build artifact rather than to the host environment. The intended behavior is instead to keep the plugin’s backend choice fixed for the lifetime of the plugin instance while selecting the concrete implementation at runtime based on host availability.

The runtime selection rule is intentionally simple for the initial implementation: if `/usr/lib/bluetoothsdk/librdk_bluetooth.so` exists, choose the bluetooth-sdk implementation; otherwise choose the BTMgr implementation.

**Related:** this change covers the runtime half only. The build and packaging half — how a single prebuilt middleware artifact can link the SDK and load on products that do not have it — is covered by `bluetooth-sdk-stub-packaging`, which also preserves the truthfulness of the path tested here. The two compose; neither supersedes the other.

## Goals / Non-Goals

**Goals:**
- Let the plugin choose a concrete backend when it initializes.
- Keep the plugin stable for its lifetime once the backend is selected.
- Preserve the current `IBtAdapter` abstraction and external plugin API.
- Keep the test injection pattern working for L1/L2 builds.

**Non-Goals:**
- Allow backend switching after initialization.
- Change the JSON-RPC surface or API semantics.
- Add a generalized backend registry or dynamic plugin architecture.
- Make the runtime resolver depend on external configuration files or network checks.

## Decisions

### 1. Runtime selection happens once at initialization

The plugin will resolve the backend as part of the initialization flow and will not re-evaluate it later. This avoids mixing callback ownership, lifecycle state, or handle semantics between the legacy and SDK implementations.

**Reasoning:**
- Both implementations have different event registration lifecycles.
- They maintain different device state and callback ownership.
- The plugin currently assumes a single active backend instance at a time.

**Alternatives considered:**
- Switch dynamically at runtime: rejected because it risks dangling callbacks and re-initialization complexity.
- Compile-time selection only: rejected because it does not satisfy the runtime filesystem policy requirement.

### 2. Both backend implementations remain compiled into the same plugin binary

The build should include both `BtSdkAdapterImpl` and `BtMgrAdapterImpl` in the library so that runtime selection is not forced by CMake configuration.

**Reasoning:**
- The runtime file check is an environment decision, not a build decision.
- This preserves compatibility across deployment images and avoids requiring separate artifacts for different environments.

**Alternatives considered:**
- Build only one implementation per binary: rejected because it forces the decision to the image build and prevents runtime detection.
- Use a dynamic loader to resolve the SDK library at runtime: rejected for now because the requirement is deliberately limited to a simple filesystem existence check and the architecture already has a clean adapter boundary.

### 3. Place the selection logic in the adapter factory layer

The default production selection should happen in `BtAdapter` instead of in `Bluetooth.cpp` itself. `BtAdapter` already acts as the dispatch layer and is the natural place to replace a compile-time static implementation pointer with a runtime-resolved one.

**Implementation shape:**
- `BtAdapter::getImpl()` checks whether `impl` is already set.
- If unset, it calls a runtime resolver function.
- The resolver checks whether `/usr/lib/bluetoothsdk/librdk_bluetooth.so` exists.
- It returns either the `BtSdkAdapterImpl` singleton or the `BtMgrAdapterImpl` singleton.
- The selected instance is then cached for the lifetime of the plugin.

**Reasoning:**
- This keeps the runtime choice localized and makes the plugin call path unchanged.
- It preserves the existing design where `Bluetooth.cpp` calls `m_btAdapter.*` without backend-specific logic.

### 4. Test injection remains authoritative in L1/L2 builds

The existing `BtAdapter::setImpl(...)` pattern remains intact. Test code should still be able to override the default implementation during setup and teardown.

**Reasoning:**
- Existing L1/L2 mock harnesses are already built around this mechanism.
- The runtime resolver is only for the production default path.

## Risks / Trade-offs

- [Runtime selection tied to filesystem path] → Mitigation: only evaluate once at initialization and document the exact path contract.
- [Mixed environments with partial SDK install] → Mitigation: default to BTMgr when the SDK library is absent; no runtime fallback after initialization.
- [Backend-specific callback state mismatch] → Mitigation: selection happens once and is not changed mid-life; deinit is the required boundary for a future backend change.
- [Build complexity from compiling both implementations] → Mitigation: keep the backend implementations isolated behind `IBtAdapter`; no public API change required.

## Migration Plan

1. Update the production build to compile both backends into the same library.
2. Add the runtime resolver in the adapter layer.
3. Replace the compile-time `BLUETOOTH_USE_SDK` default path with a runtime-selected instance.
4. Keep `BtAdapter::setImpl()` override behavior for tests.
5. Validate the plugin initializes correctly with the SDK present and with the SDK absent.

## Open Questions

- Should the runtime file check be implemented in a small helper function in the adapter layer or at the plugin initialization boundary?
- Do we want to log which backend was selected during initialization for easier diagnostics and support scenarios?
