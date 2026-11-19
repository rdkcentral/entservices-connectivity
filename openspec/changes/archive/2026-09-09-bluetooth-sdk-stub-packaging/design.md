## Context

The common `entservices-connectivity` IPK must run on products that do and do not ship the real Bluetooth SDK. BTMgr is guaranteed on every Bluetooth-enabled product. The real SDK exists only on some products and is installed at `/usr/lib/bluetoothsdk/librdk_bluetooth.so`.

The previous stub-provider approach linked the SDK directly into the main plugin and swapped a no-op SDK provider onto legacy images. It was discarded because it required copied C++ SDK ABI, `sdbus-c++` on legacy products, virtual-provider configuration, and vendor-layer recipe changes.

## Decisions

### 1. SDK code lives in a private dynamically loaded module

`BtSdkAdapterImpl`, `EventBridge`, `AuthBridge`, and `DeviceTypeClassifier` build into `libBluetoothSdkBackend.so`, installed beside the Bluetooth plugin. The main Bluetooth plugin contains `BtAdapter` and `BtMgrAdapterImpl` but no SDK headers, source files, or direct link dependency.

The module exports a narrow C ABI:

```cpp
extern "C" IBtAdapter* CreateBluetoothSdkAdapter();
extern "C" void DestroyBluetoothSdkAdapter(IBtAdapter*);
```

The factory boundary uses the existing SDK-free `IBtAdapter` interface. The module owns destruction so allocation and deletion remain in the same shared object.

### 2. Load only after confirming the real SDK marker

On initialization, `BtAdapter` checks for the real SDK marker. When present it loads that exact path using `RTLD_NOW | RTLD_GLOBAL`, then loads the private backend module using `RTLD_NOW | RTLD_LOCAL`, resolves the factory functions with `dlsym`, and creates the adapter. When absent, it selects BTMgr and does not attempt either SDK load.

The SDK is loaded first to satisfy the module's SDK symbols without requiring RPATH, loader configuration, virtual providers, or vendor recipe changes.

### 3. Positive SDK detection plus load failure is fatal

If the real marker exists but the SDK, module, factory, or adapter initialization fails, plugin initialization fails with a diagnostic. It must not fall back to BTMgr because the marker causes `btmgr.service` to be skipped on SDK images.

### 4. Module handles remain loaded for process lifetime

On plugin deinitialization, `BtAdapter` calls `IBtAdapter::deinit()` and the module destroy function. It deliberately does not call `dlclose()` on the SDK module or real SDK handles. This avoids unloading code while SDK D-Bus callbacks or library-owned state could still be reachable.

### 5. Tests retain injection and add loader behavior coverage

`BtAdapter::setImpl()` remains authoritative for L1/L2 tests, so existing unit tests avoid dynamic loading. Loader behavior is made injectable or otherwise testable to cover marker-absent BTMgr selection, SDK load ordering, and controlled failures.

## Non-Goals

- A no-op Bluetooth SDK stub.
- A virtual SDK provider, `PREFERRED_PROVIDER` changes, or vendor recipe overlays.
- Dynamically loading BTMgr.
- Changing the public JSON-RPC interface.

## Risks

- [SDK module and common plugin ABI drift] → Both are built together from the same source and share the private `IBtAdapter` interface. Keep factory exports versioned if the module is ever independently released.
- [SDK marker exists but the module fails] → Fail initialization; do not choose an inactive BTMgr service.
- [SDK callbacks survive deinitialization] → Destroy the adapter but retain dynamic-library handles until process exit.
- [A target uses `/usr/lib64`] → Confirm target `${libdir}` during integration and extend the marker check as necessary.
