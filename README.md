====================================================================
  JSON-RPC IMPLEMENTATION MAP — ResourceManager Plugin
====================================================================

There are 6 key pieces that together wire up JSON-RPC in this plugin.
Every Thunder plugin follows this exact same pattern.

--------------------------------------------------------------------
1. INHERIT FROM PluginHost::JSONRPC  [ResourceManager.h, line 29]
--------------------------------------------------------------------

    class ResourceManager : public PluginHost::IPlugin,
                            public PluginHost::JSONRPC   <-- THIS

  PluginHost::JSONRPC is the base class that provides:
    - The Register() method (used in step 3)
    - The internal dispatch table that maps method name strings to handlers
    - The wire protocol handling (receives JSON, routes, sends response)

--------------------------------------------------------------------
2. EXPOSE THE DISPATCHER VIA INTERFACE MAP  [ResourceManager.h, line 59-62]
--------------------------------------------------------------------

    BEGIN_INTERFACE_MAP(ResourceManager)
        INTERFACE_ENTRY(PluginHost::IPlugin)
        INTERFACE_ENTRY(PluginHost::IDispatcher)   <-- THIS
    END_INTERFACE_MAP

  IDispatcher is the interface Thunder queries to send JSON-RPC calls
  into your plugin. Without INTERFACE_ENTRY(PluginHost::IDispatcher),
  Thunder cannot find the JSON-RPC handler and no calls will ever arrive.

--------------------------------------------------------------------
3. DECLARE METHOD NAME CONSTANTS  [ResourceManager.h lines 46-48 / .cpp lines 11-13]
--------------------------------------------------------------------

  Header (declaration):
    static const string METHOD_GET_API_VERSION_NUMBER;
    static const string METHOD_GET_SYSTEM_RESOURCE_INFO;
    static const string METHOD_GET_STATE;

  CPP (definition):
    const string ResourceManager::METHOD_GET_API_VERSION_NUMBER = "getApiVersionNumber";
    const string ResourceManager::METHOD_GET_SYSTEM_RESOURCE_INFO = "getSystemResourceInfo";
    const string ResourceManager::METHOD_GET_STATE = "getState";

  These are the exact strings a caller uses over the wire:
    { "method": "org.rdk.ResourceManagerTop.getState" }
  The part after the dot must match these string values exactly.

--------------------------------------------------------------------
4. REGISTER METHODS IN CONSTRUCTOR  [resourcemanager.cpp, lines 45-48]
--------------------------------------------------------------------

    Register(METHOD_GET_API_VERSION_NUMBER, &ResourceManager::getApiVersionNumber, this);
    Register(METHOD_GET_SYSTEM_RESOURCE_INFO, &ResourceManager::getSystemResourceInfo, this);
    Register(METHOD_GET_STATE, &ResourceManager::getState, this);

  Register() is inherited from PluginHost::JSONRPC.
  It builds an internal lookup table:
    "getApiVersionNumber"   --> &ResourceManager::getApiVersionNumber
    "getSystemResourceInfo" --> &ResourceManager::getSystemResourceInfo
    "getState"              --> &ResourceManager::getState

  When a JSON-RPC call arrives, Thunder looks up the method name
  in this table and calls the matching function pointer.

--------------------------------------------------------------------
5. METHOD SIGNATURE — THE CONTRACT  [resourcemanager.cpp, lines 98+]
--------------------------------------------------------------------

    uint32_t ResourceManager::getState(
        const JsonObject& parameters,   <-- input from the caller
        JsonObject& response            <-- output sent back to the caller
    )

  Rules that must be followed:
    - Return type MUST be uint32_t
    - First param MUST be const JsonObject& (read-only input)
    - Second param MUST be JsonObject& (writable output)
    - Return Core::ERROR_NONE (0) on success
    - Return Core::ERROR_GENERAL / Core::ERROR_UNAVAILABLE etc. on failure

--------------------------------------------------------------------
6. FILLING THE RESPONSE  [resourcemanager.cpp, inside each method]
--------------------------------------------------------------------

  Whatever you put into the response JsonObject is serialized to JSON
  and sent back to the caller:

    response["state"]   = "active";     --> { "state": "active" }
    response["success"] = true;         --> { "success": true }
    response["version"] = m_apiVersionNumber; --> { "version": 1 }

  Convention used in this repo:
    - Always include "success": true/false
    - On error, include "message": "reason string"
    - Return the matching Core::ERROR_* code

====================================================================
  CALL FLOW SUMMARY
====================================================================

  External caller (e.g. Thunder client / test tool)
       |
       | JSON: { "method": "org.rdk.ResourceManagerTop.getState", "params": {} }
       v
  Thunder framework receives it
       |
       | looks up "getState" in the dispatch table (step 4)
       v
  ResourceManager::getState(parameters, response) is called
       |
       | fills response JsonObject
       v
  Thunder serializes response to JSON and sends it back
       |
       v
  Caller receives: { "state": "active", "success": true }

====================================================================
  FILES INVOLVED
====================================================================

  ResourceManager.h      -- steps 1, 2, 3 (declarations)
  resourcemanager.cpp    -- steps 3, 4, 5, 6 (definitions + logic)
  UtilsJsonRpc.h         -- provides LOGINFOMETHOD() helper macro
  Module.h               -- provides PluginHost::IPlugin, JSONRPC base classes

====================================================================



Here's the short version of the 6 key pieces:

| # | What | Where | Role |
|---|---|---|---|
| 1 | `public PluginHost::JSONRPC` | ResourceManager.h | Gives the class JSON-RPC capability |
| 2 | `INTERFACE_ENTRY(PluginHost::IDispatcher)` | ResourceManager.h | Tells Thunder "yes, I accept JSON-RPC calls" |
| 3 | `static const string METHOD_*` | `.h` + `.cpp` | The exact method name strings callers use over the wire |
| 4 | `Register(METHOD_*, &handler, this)` | Constructor in `.cpp` | Builds the name→function lookup table |
| 5 | `uint32_t method(const JsonObject& parameters, JsonObject& response)` | `.cpp` | The mandatory signature for every handler |
| 6 | `response["key"] = value` | Inside each method | What actually gets serialized and sent back |

====================================================================
## ResourceManagerTop curl commands

### Status
```bash
curl.exe -d '{"jsonrpc":"2.0","id":2,"method":"Controller.1.status@org.rdk.ResourceManagerTop"}' http://127.0.0.1:9998/jsonrpc
```

### Activate
```bash
curl.exe -d '{"jsonrpc":"2.0","id":1,"method":"Controller.1.activate","params":{"callsign":"org.rdk.ResourceManagerTop"}}' http://127.0.0.1:9998/jsonrpc
```

### getApiVersionNumber
```bash
curl.exe -d '{"jsonrpc":"2.0","id":10,"method":"org.rdk.ResourceManagerTop.1.getApiVersionNumber","params":{}}' http://127.0.0.1:9998/jsonrpc
```

### getSystemResourceInfo
```bash
curl.exe -d '{"jsonrpc":"2.0","id":11,"method":"org.rdk.ResourceManagerTop.1.getSystemResourceInfo","params":{}}' http://127.0.0.1:9998/jsonrpc
```

### getState
```bash
curl.exe -d '{"jsonrpc":"2.0","id":12,"method":"org.rdk.ResourceManagerTop.1.getState","params":{}}' http://127.0.0.1:9998/jsonrpc
```
