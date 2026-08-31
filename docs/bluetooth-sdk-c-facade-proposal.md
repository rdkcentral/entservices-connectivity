# Proposal: Stable C ABI facade for librdk_bluetooth.so

## Problem

`entservices-connectivity`'s Bluetooth plugin is one common middleware IPK shipped
to every product, including products that don't have the Bluetooth SDK installed.
It selects its backend at runtime by checking whether
`/usr/lib/bluetoothsdk/librdk_bluetooth.so` exists, so it can never link that
library at build time (no `DT_NEEDED`), and it can't build a per-product variant —
there is exactly one build of this component for all products.

Because `bluetooth::Adapter` and `bluetooth::Device` are concrete C++ classes that
inherit `sdbus-c++` proxy interfaces, any code that safely calls through them must
be compiled against their real definitions (real memory layout, real vtable). That
requires the real `bluetooth-sdk` headers and `sdbus-c++` at build time — which
conflicts with the "one build, no SDK dependency" constraint above. A pure
`dlopen()`-and-hope approach compiled against stand-in headers is unsafe: if the
stand-ins don't reproduce the exact base classes and vtable layout of the real
classes, virtual calls on objects returned by the real SDK are undefined behavior.

## Ask

Export a small, stable, `extern "C"` API from `librdk_bluetooth.so` that the
plugin can `dlopen()` + `dlsym()` at runtime, with no C++ classes, no
`sdbus-c++` types, and no vtables crossing the boundary. Plain C symbols have
stable (unmangled) names and a fixed calling convention, so `dlsym()` is safe
regardless of what compiler or STL either side used — this sidesteps the ABI
problem entirely instead of trying to work around it.

## Shape of the facade

- **Opaque handles**, not exposed structs, for `Manager`/`Adapter`/`Device`. Their
  internal representation (including any `sdbus-c++` usage) stays entirely
  inside `librdk_bluetooth.so` and can change freely without breaking callers.
- **POD-only data crossing the boundary** — no `std::string`, `std::vector`,
  `std::function`, `std::shared_ptr`. Strings as `const char*`, collections as
  `(ptr, count)` pairs, callbacks as a plain function pointer + `void* userdata`.
- **One well-known exported symbol**, `BtSdkCApi_GetTable()`, returning a
  versioned struct of function pointers. This means we only need a single
  `dlsym()` call to resolve everything, and adding functions later doesn't
  require us to `dlsym()` new symbol names — just extend the table (with an
  `abiVersion` field we can check to be sure it's the shape we expect).
- **Explicit ownership rules** per function (who allocates/frees what), since
  there's no RAII across a C boundary.

## Illustrative sketch (not the final API)

```c
// Pure C. No C++ classes, no sdbus-c++, no vtables. Safe to vendor permanently
// into entservices-connectivity, since it doesn't change with the SDK internals.

typedef struct BtSdkManager_* BtSdkManagerHandle;
typedef struct BtSdkAdapter_* BtSdkAdapterHandle;

typedef enum { BTSDK_OK = 0, BTSDK_ERROR = 1, BTSDK_NOT_SUPPORTED = 2 } BtSdkStatus;

typedef enum {
    BTSDK_EVENT_DISCOVERY_STARTED, BTSDK_EVENT_DISCOVERY_STOPPED,
    BTSDK_EVENT_POWERED_ON, BTSDK_EVENT_POWERED_OFF,
    BTSDK_EVENT_DEVICE_DISCOVERED, BTSDK_EVENT_DEVICE_DISAPPEARED,
} BtSdkAdapterEvent;

typedef struct {
    const char* mac;
    const char* name;
    uint32_t    classOfDevice;
    uint16_t    appearance;
    const char* const* uuids;
    int         uuidCount;
    int         paired;
    int         connected;
} BtSdkDeviceInfo;

typedef void (*BtSdkAdapterEventCb)(void* userdata, BtSdkAdapterEvent event, const BtSdkDeviceInfo* device);
typedef int  (*BtSdkAuthRequestCb)(void* userdata, int authType, const char* mac, const char* deviceType);

typedef struct {
    uint32_t abiVersion;

    BtSdkManagerHandle (*ManagerCreate)(BtSdkAuthRequestCb authCb, void* authUserdata);
    void               (*ManagerDestroy)(BtSdkManagerHandle mgr);
    BtSdkStatus        (*ManagerGetDefaultAdapter)(BtSdkManagerHandle mgr, BtSdkAdapterHandle* outAdapter);

    BtSdkStatus (*AdapterGetPowered)(BtSdkAdapterHandle adapter, int* outPowered);
    BtSdkStatus (*AdapterSetPowered)(BtSdkAdapterHandle adapter, int powered);
    BtSdkStatus (*AdapterStartScan)(BtSdkAdapterHandle adapter, const char* profile);
    BtSdkStatus (*AdapterStopScan)(BtSdkAdapterHandle adapter);
    BtSdkStatus (*AdapterRegisterEvents)(BtSdkAdapterHandle adapter, BtSdkAdapterEventCb cb, void* userdata);
    BtSdkStatus (*AdapterUnregisterEvents)(BtSdkAdapterHandle adapter);
    BtSdkStatus (*AdapterGetDevices)(BtSdkAdapterHandle adapter, int state,
                                     BtSdkDeviceInfo* outDevices, int maxCount, int* outCount);

    BtSdkStatus (*DevicePair)(BtSdkAdapterHandle adapter, const char* mac);
    BtSdkStatus (*DeviceUnpair)(BtSdkAdapterHandle adapter, const char* mac);
    BtSdkStatus (*DeviceConnect)(BtSdkAdapterHandle adapter, const char* mac, const char* deviceType);
    BtSdkStatus (*DeviceDisconnect)(BtSdkAdapterHandle adapter, const char* mac, const char* deviceType);
} BtSdkCApiTable;

// The only symbol we dlsym() by name.
const BtSdkCApiTable* BtSdkCApi_GetTable(void);
```

## What this buys both sides

- The SDK team keeps their real C++ implementation entirely internal and free
  to evolve (including `sdbus-c++` usage) without ever breaking the facade's
  binary compatibility.
- `entservices-connectivity` needs only this tiny, dependency-free header to
  build — no `sdbus-c++`, no real `bluetooth-sdk` C++ headers, no per-product
  build variants. The header can be vendored into the plugin's own source tree.
- Runtime activation stays exactly as designed: check the marker file, and
  only if present, `dlopen()` the real library and resolve this one symbol.

## What we need from the SDK team

1. Agreement on (or a counter-proposal for) the function surface above, scoped
   to what `BtSdkAdapterImpl` actually needs (adapter power/name/discoverable,
   scan start/stop, device list/pair/unpair/connect/disconnect, device
   properties, adapter + auth events). Audio control (`setAudioStream`, etc.)
   is out of scope for now — it's already unimplemented pending a future SDK
   audio module (see existing `T-7` TODOs).
2. An `abiVersion` constant we can check immediately after `dlopen()`, bumped
   only on breaking changes to the table's layout.
3. Confirmation of memory-ownership rules per call (e.g. does
   `AdapterGetDevices` require caller-allocated arrays, as sketched above, or
   does the SDK allocate and provide a free function?).
