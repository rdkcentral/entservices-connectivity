# BartonMatter Plugin — Event Subscription Guide

## Overview

The BartonMatter plugin fires two JSON-RPC notifications that UI apps can subscribe to:

| Event name | Fired when |
|---|---|
| `onDeviceCommissioned` | A new Matter device completes commissioning and is added to the network |
| `onDeviceStateChanged` | A commissioned device changes any attribute (on/off, brightness, lock state, temperature, etc.) |

---

## How to Subscribe

Use the Thunder JSON-RPC `register` method on the `org.rdk.BartonMatter.1` plugin.

### Subscribe to both events

```bash
# Subscribe to onDeviceCommissioned
curl -s -H "Content-Type: application/json" \
  --request POST \
  --data '{"jsonrpc":"2.0","id":1,"method":"org.rdk.BartonMatter.1.register","params":{"event":"onDeviceCommissioned","id":"myClientId"}}' \
  http://127.0.0.1:9998/jsonrpc

# Subscribe to onDeviceStateChanged
curl -s -H "Content-Type: application/json" \
  --request POST \
  --data '{"jsonrpc":"2.0","id":2,"method":"org.rdk.BartonMatter.1.register","params":{"event":"onDeviceStateChanged","id":"myClientId"}}' \
  http://127.0.0.1:9998/jsonrpc
```

### Unsubscribe

```bash
curl -s -H "Content-Type: application/json" \
  --request POST \
  --data '{"jsonrpc":"2.0","id":3,"method":"org.rdk.BartonMatter.1.unregister","params":{"event":"onDeviceStateChanged","id":"myClientId"}}' \
  http://127.0.0.1:9998/jsonrpc
```

---

## Event Payloads

### `onDeviceCommissioned`

Fired once when a device's commissioning sequence completes and Barton adds it to the device database.

```json
{
  "jsonrpc": "2.0",
  "method": "org.rdk.BartonMatter.1.onDeviceCommissioned",
  "params": {
    "nodeId":      "50821013fad3ff92",
    "deviceClass": "matterDimmableLight"
  }
}
```

| Field | Type | Description |
|---|---|---|
| `nodeId` | string | 64-bit Matter node ID as a hex string (matches the id used in `WriteResource`, `ReadResource`, etc.) |
| `deviceClass` | string | Barton device class, e.g. `matterDimmableLight`, `matterDoorLock`, `matterPlug` |

---

### `onDeviceStateChanged`

Fired every time any resource (attribute) of a commissioned device changes. This includes physical actions (someone manually flipping a switch), remote writes via `WriteResource`, or periodic attribute reports from the device.

```json
{
  "jsonrpc": "2.0",
  "method": "org.rdk.BartonMatter.1.onDeviceStateChanged",
  "params": {
    "nodeId":       "b5cb337bc3be548d",
    "resourceType": "isOn",
    "value":        "true"
  }
}
```

| Field | Type | Description |
|---|---|---|
| `nodeId` | string | 64-bit Matter node ID as a hex string |
| `resourceType` | string | The resource/attribute that changed. Common values below. |
| `value` | string | New value as a string (always stringified, even for booleans & numbers) |

#### Common `resourceType` values

| `resourceType` | Device type | Example values |
|---|---|---|
| `isOn` | Light, plug, switch | `"true"` / `"false"` |
| `currentLevel` | Dimmable light | `"0"` – `"254"` |
| `colorXY` | Color light | `"24939,24701"` (CIE 1931 x,y × 65535) |
| `locked` | Door lock | `"true"` (locked) / `"false"` (unlocked) |
| `label` | Any | Human-readable device label |

---

## End-to-End Example (WebSocket / Thunder.js)

```javascript
const ws = new WebSocket("ws://127.0.0.1:9998/jsonrpc");

ws.onopen = () => {
  // Subscribe
  ws.send(JSON.stringify({
    jsonrpc: "2.0", id: 1,
    method: "org.rdk.BartonMatter.1.register",
    params: { event: "onDeviceStateChanged", id: "ui-app" }
  }));
};

ws.onmessage = (msg) => {
  const data = JSON.parse(msg.data);
  if (data.method === "org.rdk.BartonMatter.1.onDeviceStateChanged") {
    const { nodeId, resourceType, value } = data.params;
    console.log(`Device ${nodeId}: ${resourceType} = ${value}`);
    // Update your UI here
  }
  if (data.method === "org.rdk.BartonMatter.1.onDeviceCommissioned") {
    const { nodeId, deviceClass } = data.params;
    console.log(`New device commissioned: ${nodeId} (${deviceClass})`);
  }
};
```

---

## What you must add to `IBartonMatter.h`

You need to add the following declarations to the `Exchange::IBartonMatter` interface in the separate interface repo. The implementation in this repo is coded against these exact signatures:

```cpp
// Nested notification interface — implemented by BartonMatterPlugin, called cross-process via COM-RPC
struct INotification : virtual public Core::IUnknown {
    enum { ID = Exchange::ID_BARTON_MATTER_NOTIFICATION }; // add this ID to Ids.h
    virtual ~INotification() = default;

    // @brief Fired when a new Matter device completes commissioning.
    // @param nodeId      64-bit Matter node ID as hex string
    // @param deviceClass Barton device class (e.g. "matterDimmableLight")
    virtual void OnDeviceCommissioned(const std::string& nodeId /* @in */,
                                      const std::string& deviceClass /* @in */) = 0;

    // @brief Fired when a commissioned device attribute changes.
    // @param nodeId       64-bit Matter node ID as hex string
    // @param resourceType Resource/attribute name (e.g. "isOn", "locked", "currentLevel")
    // @param value        New value as string
    virtual void OnDeviceStateChanged(const std::string& nodeId /* @in */,
                                      const std::string& resourceType /* @in */,
                                      const std::string& value /* @in */) = 0;
};

// Register/unregister a notification sink (called by BartonMatterPlugin on Initialize/Deinitialize)
virtual Core::hresult Register(INotification* sink /* @in */) = 0;
virtual Core::hresult Unregister(INotification* sink /* @in */) = 0;
```

You also need to add `ID_BARTON_MATTER_NOTIFICATION` to `Ids.h` (the Thunder interface ID registry), with a unique value that doesn't collide with other interfaces.
