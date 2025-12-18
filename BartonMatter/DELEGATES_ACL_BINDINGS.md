BartonMatter Technical Implementation: Delegates, ACLs, Bindings & Callbacks
============================================================================

This document explains the core C++/Matter SDK implementation details in simple terms, focusing on internal workflow.

---

## 1. Callback Delegates: Registration and Structure

### How Delegates Are Created

The plugin stores callback delegates as **member variables** (BartonMatterImplementation.h:116-117):

```cpp
chip::Callback::Callback<void (*)(void*, chip::Messaging::ExchangeManager&, const chip::SessionHandle&)> mSuccessCallback;
chip::Callback::Callback<void (*)(void*, const chip::ScopedNodeId&, CHIP_ERROR)> mFailureCallback;
```

**What these are:**
- `chip::Callback::Callback<FunctionSignature>` is a template wrapper from Matter SDK
- Stores: a function pointer + a context pointer (the `this` pointer)
- Matter SDK APIs accept these objects and invoke them when async operations complete

**Initialized in constructor** (line 43-44):
```cpp
mSuccessCallback(BartonMatterImplementation::OnSessionEstablishedStatic, this),
mFailureCallback(BartonMatterImplementation::OnSessionFailureStatic, this)
```

Simple explanation: We create two "callback tickets" when the plugin object is born. Each ticket contains:
- Which function to call (OnSessionEstablishedStatic or OnSessionFailureStatic)
- Which object instance to call it on (`this`)

### Static Trampoline Pattern

Matter SDK requires **static callbacks** (C-style function pointers). We bridge to C++ member functions using trampolines:

```cpp
// Line 774-778: Success trampoline
static void OnSessionEstablishedStatic(void * context,
                                       chip::Messaging::ExchangeManager & exchangeMgr,
                                       const chip::SessionHandle & sessionHandle)
{
    // Cast context back to our class instance
    reinterpret_cast<BartonMatterImplementation*>(context)->OnSessionEstablished(sessionHandle);
}

// Line 780-784: Failure trampoline
static void OnSessionFailureStatic(void * context,
                                   const chip::ScopedNodeId & peerId,
                                   CHIP_ERROR error)
{
    reinterpret_cast<BartonMatterImplementation*>(context)->OnSessionFailure(peerId, error);
}
```

**Flow:**
1. Matter SDK calls `OnSessionEstablishedStatic(context, ...)` where `context` is our original `this` pointer
2. We cast `context` back to `BartonMatterImplementation*`
3. Call the real member function `OnSessionEstablished()` which can access member variables

Simple explanation: Static functions can't use `this`. Matter gives us back the `context` pointer we provided during registration, we cast it to our class type, then call the real member function.

---

## 2. Access Control Lists (ACLs): Structure and Creation

### What an ACL Entry Contains

An ACL entry is a struct with these fields (from Matter SDK AccessControl.h):

```cpp
chip::Access::AccessControl::Entry entry;
```

**Key fields we populate:**
- **AuthMode**: How the subject authenticates (CASE = secure session with certificates)
- **Privilege**: What operations allowed (Administer, View, Operate, etc.)
- **FabricIndex**: Which fabric this entry belongs to
- **Subjects**: List of node IDs that are granted access
- **Targets**: List of endpoints/clusters this entry applies to

### Building an ACL Entry (AddACLEntryForClient, line 570-685)

**Step 1: Initialize entry with auth mode and privilege**
```cpp
entry.SetAuthMode(chip::Access::AuthMode::kCase);  // CASE = Certificate Authenticated Session Establishment
entry.SetPrivilege(chip::Access::Privilege::kAdminister);  // Full access
entry.SetFabricIndex(fabricIndex);  // Which fabric (Matter network)
```

Simple explanation: We're saying "this entry is for devices with valid Matter certificates (CASE), they get admin rights, on fabric #1"

**Step 2: Add the subject (the node that gets access)**
```cpp
err = entry.AddSubject(nullptr, nodeId);  // nodeId = the commissioned device's ID
```

Simple explanation: "Node 0xABCD1234 is the one getting access"

**Step 3: Add targets (which endpoints the subject can access)**

We add three targets - one for each Barton endpoint:

```cpp
// Video Player endpoint (endpoint 1)
AccessControl::Entry::Target target1 = {
    .flags = AccessControl::Entry::Target::kEndpoint,
    .endpoint = 1
};
entry.AddTarget(nullptr, target1);

// Speaker endpoint (endpoint 2)
AccessControl::Entry::Target target2 = {
    .flags = AccessControl::Entry::Target::kEndpoint,
    .endpoint = 2
};
entry.AddTarget(nullptr, target2);

// Content App endpoint (endpoint 3)
AccessControl::Entry::Target target3 = {
    .flags = AccessControl::Entry::Target::kEndpoint,
    .endpoint = 3
};
entry.AddTarget(nullptr, target3);
```

Simple explanation: "The subject can talk to endpoints 1, 2, and 3" (video player, speaker, content app)

**Step 4: Create the entry in Matter's ACL table**
```cpp
err = GetAccessControl().CreateEntry(nullptr, fabricIndex, nullptr, entry);
```

Simple explanation: We built the entry struct in memory. Now we tell Matter SDK "save this ACL rule to your persistent storage"

### ACL Storage
Matter SDK stores ACLs in KVS (Key-Value Storage). When a device tries to access an endpoint, Matter checks these ACL entries to decide allow/deny.

---

## 3. Matter Event Loop Scheduling

### Why We Need It

The plugin code runs on **Thunder's thread** (JSON-RPC handler thread). Matter SDK APIs must run on **Matter's thread** (the chip event loop thread). Calling Matter APIs from the wrong thread causes crashes or race conditions.

### How Scheduling Works (line 690-698)

**Schedule work:**
```cpp
// Store parameters in member variables first
this->mEstablishSessionNodeId = nodeId;
this->mEstablishSessionFabricIndex = fabricIndex;
this->mClientDeviceUuid = deviceUuid;

// Schedule work on Matter's thread
chip::DeviceLayer::PlatformMgr().ScheduleWork(&BartonMatterImplementation::EstablishSessionWork,
                                               reinterpret_cast<intptr_t>(this));
```

**Work function executes on Matter's thread:**
```cpp
static void EstablishSessionWork(intptr_t context)
{
    auto *self = reinterpret_cast<BartonMatterImplementation *>(context);
    chip::Server & server = chip::Server::GetInstance();

    // Create ScopedNodeId from stored member variables
    chip::ScopedNodeId peerNode(self->mEstablishSessionNodeId, self->mEstablishSessionFabricIndex);

    // Ask CASE session manager to establish session
    server.GetCASESessionManager()->FindOrEstablishSession(peerNode,
                                                           &self->mSuccessCallback,
                                                           &self->mFailureCallback);
}
```

Simple explanation:
1. We can't call Matter APIs from Thunder's thread
2. We save the nodeId/fabricIndex in member variables (mEstablishSessionNodeId, etc.)
3. We post a task to Matter's work queue using `ScheduleWork`
4. Matter's thread picks up the task and runs `EstablishSessionWork`
5. That function reads the saved member variables and calls CASE session manager
6. CASE manager will later call our success/failure callbacks

---

## 4. CASE Session Establishment

### What CASE Is

CASE = Certificate Authenticated Session Establishment. It's Matter's protocol for:
- Mutual authentication using certificates
- Deriving session keys for encrypted communication
- Opening a secure channel between two nodes

### The FindOrEstablishSession Call

```cpp
server.GetCASESessionManager()->FindOrEstablishSession(peerNode, &mSuccessCallback, &mFailureCallback);
```

**What happens internally:**
1. CASE manager checks if we already have a session with this node
2. If yes: invoke success callback immediately with existing session handle
3. If no: start CASE protocol (certificate exchange, sigma protocol, key derivation)
4. When CASE completes: invoke success callback
5. If CASE fails (timeout, bad cert, etc.): invoke failure callback

**Callback invocation:**
- Success: Matter calls `mSuccessCallback` → `OnSessionEstablishedStatic()` → `OnSessionEstablished()`
- Failure: Matter calls `mFailureCallback` → `OnSessionFailureStatic()` → `OnSessionFailure()`

---

## 5. Session Established Callback: Writing Bindings

### OnSessionEstablished Implementation (line 786-846)

When the session is ready, we write bindings to the client device:

```cpp
void OnSessionEstablished(const chip::SessionHandle & sessionHandle)
{
    // Get ExchangeManager for creating WriteClient
    chip::Messaging::ExchangeManager * exchangeMgr = &chip::Server::GetInstance().GetExchangeManager();

    // Get fabric info to retrieve local node ID
    chip::FabricIndex fabricIndex = sessionHandle->GetFabricIndex();
    const chip::FabricInfo * fabricInfo = chip::Server::GetInstance().GetFabricTable().FindFabricWithIndex(fabricIndex);
    chip::NodeId localNodeId = fabricInfo->GetNodeId();

    // Barton's accessible endpoints
    std::vector<chip::EndpointId> endpoints = { 1, 2, 3 };

    // Write bindings to client device
    WriteClientBindings(*exchangeMgr, sessionHandle, localNodeId, endpoints);
}
```

Simple explanation:
1. Session is ready - we have an encrypted channel
2. Get our own node ID from fabric table
3. Prepare list of endpoints to expose (1=video, 2=speaker, 3=content app)
4. Call WriteClientBindings to tell the client device about these endpoints

---

## 6. Writing Binding Cluster Attribute

### WriteClientBindings Implementation (line 700-772)

**Step 1: Build binding list**
```cpp
std::vector<Structs::TargetStruct::Type> bindings;

for (chip::EndpointId endpoint : endpoints)  // endpoints = {1, 2, 3}
{
    bindings.push_back(Structs::TargetStruct::Type{
        .node        = MakeOptional(localNodeId),      // Our node ID (Barton)
        .group       = NullOptional,                   // Not a group binding
        .endpoint    = MakeOptional(endpoint),         // Endpoint 1, 2, or 3
        .cluster     = NullOptional,                   // All clusters on endpoint
        .fabricIndex = chip::kUndefinedFabricIndex,    // Let SDK fill this in
    });
}
```

Simple explanation: We create a list of binding structs. Each struct says "you can talk to node X (Barton), endpoint Y". We're creating 3 bindings for endpoints 1, 2, 3.

**Step 2: Create WriteClient**
```cpp
auto writeClient = new chip::app::WriteClient(&exchangeMgr, nullptr, chip::Optional<uint16_t>::Missing());
```

Simple explanation: WriteClient is Matter's API for writing attributes to remote devices. We create one and give it the exchange manager.

**Step 3: Set up attribute path**
```cpp
chip::app::AttributePathParams attributePathParams;
attributePathParams.mEndpointId = 1;  // Client device's endpoint
attributePathParams.mClusterId = chip::app::Clusters::Binding::Id;  // Binding cluster
attributePathParams.mAttributeId = chip::app::Clusters::Binding::Attributes::Binding::Id;  // Binding attribute
```

Simple explanation: We're specifying "write to endpoint 1, Binding cluster, Binding attribute" on the remote device.

**Step 4: Encode and send**
```cpp
// Create attribute value with our bindings list
chip::app::Clusters::Binding::Attributes::Binding::TypeInfo::Type bindingListAttr(bindings.data(), bindings.size());

// Encode into write request
err = writeClient->EncodeAttribute(attributePathParams, bindingListAttr);

// Send write request over the session
err = writeClient->SendWriteRequest(sessionHandle);
```

Simple explanation:
1. Wrap our bindings list in a typed attribute object
2. Encode it into the write request (TLV encoding)
3. Send the request over the CASE session to the device

The device receives this write, updates its Binding cluster, and now knows it can talk to Barton's endpoints 1, 2, 3.

---

## 7. Key Press and App Launch Callbacks

### How Key Press Events Flow to Barton

When the **client device** sends a key press (e.g., SELECT, UP, DOWN):

**Step 1: Client sends Invoke Command**
Client uses Matter Interaction Model to send:
- Cluster: KeypadInput (0x0509)
- Command: SendKey
- Parameters: { keyCode: KEY_SELECT }

**Step 2: Matter SDK receives command on Barton**
Matter SDK looks up the cluster delegate for KeypadInput on the target endpoint.

**Step 3: Delegate handles command**
```cpp
// From MatterClusterDelegates (registered during plugin init)
class KeypadInputDelegate : public chip::app::Clusters::KeypadInput::Delegate
{
    void HandleSendKey(KeyCodeEnum keyCode) override
    {
        // Convert Matter key code to Barton key code
        // Call b_core_client APIs or emit Thunder events
        // UI receives key event
    }
};
```

Simple explanation: Matter SDK dispatches the command to our registered delegate. The delegate converts Matter key codes to Barton/Thunder events and forwards to UI layer.

### Delegate Registration

Delegates are registered during plugin initialization:

```cpp
// During InitializeCommissioner or plugin construction
chip::app::Clusters::KeypadInput::SetDelegate(endpointId, &keypadDelegate);
chip::app::Clusters::ApplicationLauncher::SetDelegate(endpointId, &appLauncherDelegate);
```

Simple explanation: We tell Matter SDK "for endpoint X KeypadInput cluster, call this delegate object when commands arrive".

---

## 8. Complete Flow Diagram

```
[JSON-RPC Handler Thread]
         |
         | CommissionDevice completes
         |
         v
    DeviceAddedHandler (GLib signal)
         |
         | Get device UUID, convert to nodeId
         |
         v
    AddACLEntryForClient()
         |
         | 1. Create AccessControl::Entry struct
         | 2. Set auth mode (CASE), privilege (Admin), fabric
         | 3. AddSubject(nodeId)
         | 4. AddTarget for endpoints 1, 2, 3
         | 5. CreateEntry() → ACL stored in KVS
         |
         | 6. Store nodeId/fabricIndex in members
         | 7. ScheduleWork(EstablishSessionWork, this)
         |
         v
    ──────────────────────────────────────
    [Matter Event Loop Thread]
         |
         v
    EstablishSessionWork()
         |
         | Read nodeId/fabricIndex from members
         | Create ScopedNodeId
         |
         v
    CASESessionManager->FindOrEstablishSession(nodeId, successCb, failCb)
         |
         | ┌─────────────────────┐
         | │ CASE Protocol Runs  │
         | │ - Sigma handshake   │
         | │ - Certificate verify│
         | │ - Session key derive│
         | └─────────────────────┘
         |
         v
    [Session Ready] → Matter invokes mSuccessCallback
         |
         v
    OnSessionEstablishedStatic(context, exchangeMgr, sessionHandle)
         |
         | reinterpret_cast<...>(context)->OnSessionEstablished(sessionHandle)
         |
         v
    OnSessionEstablished(sessionHandle)
         |
         | 1. Get localNodeId from fabric table
         | 2. Prepare endpoints list {1, 2, 3}
         |
         v
    WriteClientBindings(exchangeMgr, sessionHandle, localNodeId, endpoints)
         |
         | 1. Build vector of TargetStruct (3 bindings)
         | 2. Create WriteClient
         | 3. Set AttributePathParams (endpoint=1, cluster=Binding, attr=Binding)
         | 4. EncodeAttribute(bindings)
         | 5. SendWriteRequest(sessionHandle)
         |
         v
    ──────────────────────────────────────
    [Network: Matter TLV message to client device]
         |
         v
    [Client Device Receives Write]
         |
         | Updates Binding cluster attribute
         | Now knows: "I can send commands to Barton endpoints 1, 2, 3"
         |
         v
    [Later: Client sends key press or app launch]
         |
         v
    ──────────────────────────────────────
    [Barton receives Invoke Command]
         |
         v
    Matter SDK dispatches to cluster delegate
         |
         v
    KeypadInputDelegate->HandleSendKey(keyCode)
         |
         | Convert to Thunder event
         |
         v
    UI receives key event and acts on it
```

---

## 9. Member Variables Used for Async State

```cpp
// Session establishment state (BartonMatterImplementation.h:120-121)
uint64_t mEstablishSessionNodeId = 0;
chip::FabricIndex mEstablishSessionFabricIndex = 0;

// Client device info for ACL/binding
std::string mClientDeviceUuid;
uint16_t mClientVendorId = 0;
uint16_t mClientProductId = 0;

// Callback delegates (initialized in constructor)
chip::Callback::Callback<...> mSuccessCallback;
chip::Callback::Callback<...> mFailureCallback;
```

**Why these exist:**
- Thunder thread sets nodeId/fabricIndex, then schedules Matter thread work
- Matter thread reads these members when the work executes
- No thread-safe queue needed because each async operation uses unique member variables
- Callbacks are stored in members so they persist for the lifetime of the plugin object

---

## 10. Thread Safety Considerations

**Three threads in play:**
1. **Thunder thread**: Handles JSON-RPC, calls into plugin
2. **Matter thread**: Runs chip event loop, handles protocol
3. **GLib thread**: Runs Barton core client main loop, emits signals

**Synchronization mechanisms:**
- `chip::DeviceLayer::PlatformMgr().ScheduleWork()`: Cross-thread dispatch (Thunder → Matter)
- `std::mutex devicesCacheMtx`: Protects commissionedDevicesCache map
- `std::mutex networkCredsMtx`: Protects WiFi credentials
- GLib signals use idle callbacks to marshal to correct thread

**Critical rules:**
- Never call Matter SDK APIs from Thunder thread directly
- Never call Thunder APIs from Matter thread directly
- Always use ScheduleWork to post tasks to Matter thread
- Always lock mutexes when accessing shared data structures

---

## 11. Verification Commands

After building and deploying the plugin:

**Check ACL entries:**
```bash
# Matter CLI on Barton
matter accesscontrol read-entry 0 1  # fabric 1, entry 0
```

**Trigger session establishment:**
Commission a device and watch logs for:
```
Session established with Node: 0xABCD1234 on Fabric 1
Writing bindings to client for 3 Barton endpoints
```

**Verify binding write:**
On the client device, read Binding cluster:
```bash
matter binding read endpoint 1
# Should show 3 entries pointing to Barton's endpoints 1, 2, 3
```

---

Generated: December 17, 2025
File: BartonCore/entservices-connectivity/BartonMatter/DELEGATES_ACL_BINDINGS.md
