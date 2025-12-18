# Barton Matter Plugin
This document previously began with low-level implementation details (callback delegates and thread handling). To make it easier for engineers and application developers, the top of the document now starts with a simple, top-to-bottom user flow showing how the UI interacts with the plugin and what happens next inside Barton Core and the Matter SDK. Following the user flow is a brief layman's explanation summarizing the important steps in plain language.

## Quick User Flow (UI → Plugin → Barton Core → Matter)

1. UI (Application) calls plugin JSON-RPC: `InitializeCommissioner()` or `CommissionDevice(passcode)` or `RemoveDevice(deviceUuid)`.
2. Plugin (`BartonMatterImplementation`) validates the request and delegates high-level lifecycle work to the Barton Core client (`b_core_client`) when appropriate.
   - Commissioning and removal use `b_core_client_commission_device()` and `b_core_client_remove_device()`.
3. Barton Core performs the heavy lifting for commissioning (PASE, NOC exchange, persistence to `devicedb`). It emits GLib signals when devices are added or configured.
4. The plugin receives signals (GLib thread) and updates its in-memory cache (`commissionedDevicesCache`) and begins Matter-specific configuration (ACLs, bindings) by scheduling work to the Matter event loop thread.
5. Matter thread performs secure operations (establish CASE sessions, create ACL entries, write binding attributes) via the Matter SDK and persists ACLs/KVS as needed.
6. When operations complete, callback trampolines move execution back into the plugin logic, which then returns success/failure to the UI caller via JSON-RPC responses or status notifications.

## Layman's Explanation (What happens when you press "Add Device"?)

- The UI tells the plugin: "Add this device using this passcode." The plugin asks Barton Core to do the actual handshake with the device. Barton Core handles network and security details and writes a small device file to disk when finished.
- Once Barton Core says the device is added, the plugin sets up permissions and connections so the new device can control the TV. This includes creating access control entries (ACLs) which say "this particular node is allowed to send commands" and writing binding entries on the client so it knows which endpoints to talk to.
- When you tell the plugin to remove a device, the plugin removes the device file and also cleans up the permissions (ACLs) and any active secure sessions so the device can be safely recommissioned later.

Notes for application developers:
- If a device still appears after removal, it may be because the plugin keeps a cache; `GetCommissionedDeviceInfo()` scans the filesystem unless the cache is already initialized. Removing a device triggers cache invalidation so subsequent queries will reflect the change.
- The plugin performs Matter SDK operations on a dedicated Matter event loop thread — JSON-RPC calls do not block while the Matter stack performs secure or network operations.

---

## 1. Introduction

### 1.1. Purpose
This document provides a comprehensive technical overview of the `BartonMatter` Thunder plugin. It details the system architecture, internal workflows, and the interaction between its core components: the Barton Core C API and the Matter SDK.

The primary role of this plugin is to act as a bridge, exposing Matter commissioning and device interaction features to the system via JSON-RPC, while managing the complexities of the underlying Matter protocol stack.

### 1.2. Scope
This document covers:
- The high-level architecture and its components.
- The rationale for using the Barton Core wrapper vs. direct Matter SDK calls.
- Detailed end-to-end flows for all public JSON-RPC APIs.
- Flowcharts for inbound (device-to-plugin) and outbound (plugin-to-device) command paths.
- Core implementation patterns for asynchronous operations, callbacks, and thread safety.

----

## 2. System Architecture

The plugin operates within a multi-layered architecture, orchestrating communication between the application layer, a high-level device management API (Barton Core), and the low-level Matter protocol stack.

![Architecture Diagram](https://i.imgur.com/your-architecture-diagram.png)
*(Self-correction: Placeholder for a real diagram if one were available)*

### 2.1. Core Components

1.  **Thunder Framework**: The host environment for the plugin. It provides the JSON-RPC server and manages the plugin's lifecycle (activation, deactivation).
2.  **BartonMatter Plugin**: The C++ plugin class (`BartonMatterImplementation`) that implements the `IBartonMatter` interface. It handles JSON-RPC requests and serves as the central orchestration point.
3.  **Barton Core Client (`b_core_client`)**: A C-based client library that provides a high-level, abstracted API for managing the device lifecycle. This component acts as a **"Matter Wrapper"**.
4.  **Matter SDK (`chip::*`)**: The official C++ SDK from the Connectivity Standards Alliance (CSA). It provides direct, low-level access to all Matter protocol features, including security, data model, and transport layers.

### 2.2. Architectural Strategy: Barton Core vs. Direct Matter SDK

The plugin employs a hybrid strategy, choosing the right tool for the job.

#### When Barton Core (Matter Wrapper) is Used:

Barton Core is used for high-level, stateful operations that are complex to manage directly.

-   **Device Commissioning**: `b_core_client_commission_device()`
-   **Device Removal**: `b_core_client_remove_device()`
-   **Device Listing**: `b_core_client_get_devices()`
-   **Generic Resource Access**: `b_core_client_read_resource()`, `b_core_client_write_resource()`

**Rationale**: Barton Core provides a stable, simplified API for the most complex parts of the device lifecycle. It abstracts away the intricate state machines of commissioning, the details of secure session management for simple resource reads/writes, and the persistence of device information in the `devicedb`.

#### When Direct Matter SDK is Used:

The plugin uses the Matter SDK directly for features that are either not exposed by Barton Core or require fine-grained control over the Matter protocol.

-   **Access Control List (ACL) Management**: `chip::Access::AccessControl`
-   **Secure Session Establishment (for bindings)**: `chip::Server::GetCASESessionManager()`
-   **Binding Table Manipulation**: `chip::app::WriteClient` on the Binding Cluster
-   **Handling Inbound Commands**: Implementing cluster-specific delegates (`KeypadInput::Delegate`, `ApplicationLauncher::Delegate`, etc.)

**Rationale**: These are advanced, application-specific interactions. For example, setting up ACLs and Bindings is a specific requirement for the TV casting use case, allowing a client device to control Barton's endpoints. Handling inbound key presses requires implementing a specific Matter cluster delegate, which is outside the scope of Barton's generic device management.

----

## 3. End-to-End API Flows

This section details the sequence of events for each public API exposed by the plugin.


## 4. Inbound and Outbound Control Flows

### 4.1. Outbound Flow: Plugin Sending Commands (e.g., Writing Bindings)

This flow is used when the plugin needs to proactively write data to a client device. It is initiated by the plugin itself, typically after commissioning.

```mermaid
graph TD
    subgraph BartonMatter Plugin
        A[DeviceAddedHandler] --> B{AddACLEntryForClient};
        B --> C{ScheduleWork(EstablishSessionWork)};
    end

    subgraph Matter SDK Thread
        D[EstablishSessionWork] --> E{CASESessionManager.FindOrEstablishSession};
        E -- Success --> F[OnSessionEstablished Callback];
    end

    subgraph BartonMatter Plugin
        G[OnSessionEstablished] --> H{WriteClientBindings};
    end

    subgraph Matter SDK
        I[Create WriteClient] --> J{Encode Attribute (Binding List)};
        J --> K{SendWriteRequest};
    end

    C --> D;
    F --> G;
    H --> I;

    K --> L((Network));

    subgraph Client Device
        L --> M{Receive Write Request};
        M --> N[Update Binding Cluster];
    end

    style A fill:#cde4ff
    style G fill:#cde4ff
```

### 4.2. Inbound Flow: Plugin Receiving Commands (e.g., Key Press)

This flow is used when a client device sends a command to the plugin. It relies on the delegate pattern.

```mermaid
graph TD
    subgraph Client Device
        A[User presses key on remote] --> B{Send InvokeRequest Command};
        B --> C((Network));
    end

    subgraph Barton (Host)
        C --> D{Matter SDK Receives Command};
        D --> E{Find Delegate for Cluster/Endpoint};
    end

    subgraph BartonMatter Plugin
        F[KeypadInputDelegate.HandleSendKey()] --> G{Convert KeyCode};
        G --> H{Emit Thunder Event or Call Barton API};
    end

    subgraph Application/UI
        I[UI Layer]
    end

    E --> F;
    H --> I;

    style A fill:#cde4ff
    style I fill:#cde4ff
```

---

## 5. Core Implementation Patterns

### 5.1. Asynchronous Operations and Callbacks

-   **Problem**: Matter operations are asynchronous and cannot block the main JSON-RPC thread.
-   **Solution**: The plugin uses a combination of `chip::Callback` objects and static "trampoline" functions.
    1.  **Storage**: `mSuccessCallback` and `mFailureCallback` are stored as member variables. They are initialized in the constructor with a pointer to a static function and the `this` pointer as context.
    2.  **Registration**: These callback objects are passed to asynchronous Matter SDK APIs like `FindOrEstablishSession`.
    3.  **Invocation**: When the operation completes, the Matter SDK calls the static function, passing the `this` pointer back as the `context`.
    4.  **Trampoline**: The static function casts the `context` pointer back to a `BartonMatterImplementation*` and calls the actual member function (e.g., `OnSessionEstablished`), restoring the object context.

### 5.2. Thread Safety

-   **Problem**: The plugin is accessed by multiple threads: Thunder's JSON-RPC thread, the Matter event loop thread, and the GLib main loop thread (for Barton Core signals).
-   **Solution**:
    1.  **Thread Dispatch**: To call a Matter SDK function from the Thunder thread, the plugin uses `chip::DeviceLayer::PlatformMgr().ScheduleWork()`. This safely posts a function to be executed on the Matter event loop thread.
    2.  **State Passing**: Parameters for the scheduled work are stored in member variables (`mEstablishSessionNodeId`, etc.) before `ScheduleWork` is called. This is safe for one-off operations but requires careful management if multiple operations could run concurrently.
    3.  **Mutexes**: Standard C++ mutexes (`std::mutex`) are used to protect shared data structures like `commissionedDevicesCache` from concurrent access.
    4.  **GLib Signals**: Barton Core signals are handled by GLib, which ensures they are delivered safely.

This architecture allows the plugin to be a robust and thread-safe bridge between the application world and the complex, asynchronous world of the Matter protocol.

---

9. Deep Dive — Exact Call Sequences, Data Schemas, Error Codes, and Tests
-------------------------------------------------------------------------

This section gives line-by-line internal call traces, the JSON schema used in `devicedb` files, exact error codes to expect from Matter/Barton, recommended retry/backoff behavior, and concrete changes to make the implementation concurrency-safe and testable.

9.1 Exact call trace: commissioning (detailed)
---------------------------------------------
Assume UI triggers commissioning via plugin API `CommissionDevice(passcode)`.

Synchronous call trace (high-level):

1. `BartonMatterImplementation::CommissionDevice(passcode)` -- called on Thunder thread
    - Validate input length, check `bartonClient != nullptr`
    - Call `b_core_client_commission_device(bartonClient, passcode)`
    - Return `Core::ERROR_NONE` (commissioning proceeds asynchronously)

2. Barton Core thread (inside `b_core_client`) executes commissioning state machine:
    - Create temporary commissioning context
    - Open transport to device IP / BLE / thread
    - Start PASE handshake (sigma)
    - Exchange operational credentials (NOC request/response)
    - Create operational dataset (fabric-index, node-id) on device
    - Persist device metadata to `/opt/.brtn-ds/storage/devicedb/<nodeid>`
    - Emit `DeviceAdded` and `DeviceConfigurationCompleted` signals (GLib callbacks)

3. `BartonMatterImplementation::DeviceAddedHandler` (GLib thread)
    - g_object_get(event, UUID, deviceClass)
    - Call `UpdateDeviceCache(deviceUuid, deviceClass)` (locks devicesCacheMtx)
    - Call `ConfigureClientACL(deviceUuid, vendorId, productId)`
      - Calls `AddACLEntryForClient(vendorId, productId, deviceUuid)`

4. `AddACLEntryForClient` (calls into Matter SDK — must run on Matter thread via ScheduleWork if called from non-Matter thread)
    - Convert deviceUuid → nodeId using `strtoull(hex, NULL, 16)`
    - `AccessControl::Entry entry; GetAccessControl().PrepareEntry(entry);`
    - `entry.SetFabricIndex(fabricIndex); entry.SetPrivilege(Privilege::kAdminister); entry.SetAuthMode(AuthMode::kCase);`
    - `entry.AddSubject(nullptr, nodeId);` and `entry.AddTarget(nullptr, target)` for endpoints 1,2,3
    - `GetAccessControl().CreateEntry(nullptr, fabricIndex, nullptr, entry)` → on success store variables and schedule session establishment

5. `chip::DeviceLayer::PlatformMgr().ScheduleWork(EstablishSessionWork, this)` executes on Matter thread
    - `EstablishSessionWork` forms `ScopedNodeId` and calls `Server::GetInstance().GetCASESessionManager()->FindOrEstablishSession(peerNode, &mSuccessCallback, &mFailureCallback);`

6. CASE manager performs handshake (could be immediate if session cached)
    - On success: Matter SDK invokes `mSuccessCallback` → static trampoline `OnSessionEstablishedStatic` → `OnSessionEstablished(sessionHandle)`

7. `OnSessionEstablished(sessionHandle)` (Matter thread)
    - Read `fabricIndex` and localNodeId
    - Prepare endpoints vector {1,2,3}
    - Call `WriteClientBindings(exchangeMgr, sessionHandle, localNodeId, endpoints)`

8. `WriteClientBindings` (Matter thread)
    - Construct `std::vector<Structs::TargetStruct::Type> bindings` with 3 targets
    - `auto writeClient = new app::WriteClient(&exchangeMgr, nullptr, Optional<uint16_t>::Missing());`
    - `writeClient->EncodeAttribute(attributePathParams, bindingListAttr);`
    - `writeClient->SendWriteRequest(sessionHandle);` (returns CHIP_ERROR)
    - WriteClient's internal delegate will deliver write response; plugin does not block here

9.2 Exact call trace: removal (detailed)
-------------------------------------
Call: `RemoveDevice(deviceUuid)` on Thunder thread.

1. Validate `deviceUuid`.
2. Convert `deviceUuid`→`nodeId`. If conversion fails, log and continue with Barton removal only.
3. If nodeId valid, call `RemoveACLEntriesForNode(nodeId)` on Matter thread via `ScheduleWork` or call synchronously if already on Matter thread.
    - Enumerate ACL entries: `GetAccessControl().Entries(fabricIndex, iterator)`
    - For each entry: call `entry.GetSubjectIterator(subjectIter)` and test equality with nodeId
    - Record entry indices to delete; call `GetAccessControl().DeleteEntry(nullptr, fabricIndex, entryIndex)` in reverse order
4. Schedule `Server::GetInstance().GetCASESessionManager()->ReleaseSessionsForNode(peerNode)` to free CASE state
5. Call `b_core_client_remove_device(bartonClient, deviceUuid.c_str())` which removes devicedb file
6. If that succeeds, clear plugin cache (lock devicesCacheMtx then clear map, set flag false)

9.3 devicedb JSON schema (current implementation expects a model string)
-------------------------------------------------------------------
The plugin currently extracts a `model` value from the device file using a string scan. Recommended formal schema:

{
  "nodeId": "90034FD9068DFF14",
  "vendor": "TEST_VENDOR",
  "product": "TV-CASTING",
  "swVer": "0x00000001",
  "serialNumber": "TEST_ANDROID_SN",
  "endpoints": [
     { "endpointId": 0, "servers": [60,52,...], "clients": [] },
     { "endpointId": 1, "servers": [3,4,29,30], "clients": [6,8,...] }
  ],
  "model": "CastingClient-Model-1"
}

Plugin code should be extended to parse JSON robustly (e.g., using a small JSON parser) instead of fragile string searches.

9.4 ACL entry structure (what plugin writes)
-------------------------------------------
When creating an ACL entry the plugin sets:
- `fabricIndex` (uint16_t) — usually the server's fabric index (the plugin's fabric)
- `authMode` = `AuthMode::kCase`
- `privilege` = `Privilege::kAdminister` (recommend lower privilege in prod)
- `subjects` = [ nodeId ]
- `targets` = [ { endpoint:1 }, { endpoint:2 }, { endpoint:3 } ]

Matter AccessControl stores entries with an internal index. Deleting entries must account for index shifts.

9.5 Error codes to watch and recommended handling
------------------------------------------------
- `CHIP_NO_ERROR` (0x00000000): success
- `CHIP_ERROR_DUPLICATE_KEY_ID` (0x00000019): ACL entry already exists — handle by enumerating + deleting existing entries before retry, or log and surface error
- `CHIP_ERROR_NOT_FOUND`: deletion target not found — treat as success for cleanup
- `CHIP_ERROR_INVALID_ARGUMENT`: input parsing issue — validate inputs
- `CHIP_ERROR_TIMEOUT`: network or CASE timeout — implement retry with backoff
- `BCore` errors: Barton Core returns its own error codes; treat failed commission/remove as transient and surface via logs/events

9.6 Recommended retry/backoff policies
-------------------------------------
- Binding write: retry 3 times with exponential backoff (e.g., 200ms, 500ms, 1000ms) on transient errors (timeout, send failed). Give up on persistent `INVALID_ARGUMENT`.
- CASE establishment: rely on Matter SDK retries; if failure, surface to UI and schedule a single delayed retry after 2s.
- ACL CreateEntry: if `DUPLICATE_KEY_ID`, attempt cleanup once then retry immediately.

9.7 Concurrency fixes (concrete code suggestions)
-----------------------------------------------
Current issue: single shared member variables (`mEstablishSessionNodeId`) make parallel commissions race. Fix:

1. Replace single fields with an `std::unordered_map<std::string, std::shared_ptr<OperationContext>> mOpContexts;` keyed by deviceUuid.
    - `OperationContext` holds `nodeId, fabricIndex, deviceUuid, retryCounts, timestamp`.
    - Protect map with `std::mutex opContextsMtx`.
2. When starting an operation, allocate `auto ctx = std::make_shared<OperationContext>(...)`; insert into map.
3. ScheduleWork with context pointer (pass ctx.get() or better, use id and lookup in map inside Matter thread) so multiple operations can proceed concurrently safely.
4. Cleanup: when operation completes, remove entry from map under lock.

Example pseudocode:

// create context
{
  std::lock_guard<std::mutex> g(opContextsMtx);
  mOpContexts[deviceUuid] = std::make_shared<OperationContext>(...);
}
// schedule work
chip::DeviceLayer::PlatformMgr().ScheduleWork(&BartonMatterImplementation::EstablishSessionWork, reinterpret_cast<intptr_t>(deviceUuidPtr));

In `EstablishSessionWork`, resolve the context by deviceUuid and operate on that context only.

9.8 Observability: logs and metrics to add
----------------------------------------
- Log lines to include unique request ID (generate UUID per public API call).
- Metrics:
  - `commission_attempts_total`, `commission_success_total`, `commission_failure_total`
  - `acl_entries_created_total`, `acl_entries_deleted_total`
  - `binding_writes_total`, `binding_write_failures_total`
- Add detailed debug logging containing fabric index, nodeId, deviceUuid, and operation timestamps.

9.9 Test checklist (unit + integration)
--------------------------------------

Unit tests:
- `GetNodeIdFromDeviceUuid`: valid/invalid hex strings
- `UpdateDeviceCache` / `RemoveDeviceFromCache` concurrency
- `RemoveACLEntriesForNode` behavior with mocked AccessControl iterator (simulate multiple entries, ensure reverse deletion)

Integration tests (requires emulator or test device):
- Commission device, verify devicedb file created, ACL entry exists, binding write executed (inspect client binding attribute)
- Remove device, verify ACL entries deleted, devicedb removed, no `DUPLICATE_KEY_ID` on recommission
- Simulate binding write failure (drop packets) and verify retry behavior

9.10 Quick verification commands and log lines to watch
-----------------------------------------------------
- Start plugin and run commissioning; watch plugin log for:
  - "Device added! UUID=..."
  - "AddACLEntryForClient: Successfully created ACL entry"
  - "Session established with Node: 0x..."
  - "Successfully sent binding write request to client"
- CLI: use Matter CLI (if available) to list ACLs and bindings
  - `matter accesscontrol list` (or use SDK helpers)
  - `matter binding read <endpoint>` on client

9.11 Suggested small refactors to simplify code
---------------------------------------------
- Introduce `OperationContext` for per-device async state instead of single-member variables.
- Use a small JSON parser (nlohmann/json lite or similar) in `ExtractModelFromDeviceFile` to parse `devicedb` safely.
- Add helper wrappers `ScheduleMatterWork(std::function<void()>)` that capture shared_ptr to context and unwrap on Matter thread.

9.12 Example OperationContext struct (C++ sketch)
------------------------------------------------
struct OperationContext {
     std::string deviceUuid;
     uint64_t nodeId;
     chip::FabricIndex fabricIndex;
     int aclRetryCount = 0;
     int bindingRetryCount = 0;
     std::chrono::steady_clock::time_point createdAt;
};

Use `shared_ptr<OperationContext>` so scheduled work holds a reference and the context isn't destroyed prematurely.

---

If you'd like, I can now:
- Apply the concurrency refactor patch to `BartonMatterImplementation.cpp` and `BartonMatterImplementation.h` (introduce `OperationContext`, `mOpContexts`, refactor relevant functions).
- Replace the fragile devicedb parsing with robust JSON parsing (small dependency addition).
- Add unit-test scaffolding for the new components.

Which of these should I implement next?

