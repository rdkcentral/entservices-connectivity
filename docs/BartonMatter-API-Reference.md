# BartonMatter API Reference

Complete technical documentation for BartonMatter Thunder plugin APIs with flow diagrams, technical details, and layman's explanations.

---

## Table of Contents

1. [Constructor & Destructor](#1-constructor--destructor)
2. [SetWifiCredentials](#2-setwificredentials)
3. [InitializeCommissioner](#3-initializecommissioner)
4. [CommissionDevice](#4-commissiondevice)
5. [ReadResource](#5-readresource)
6. [WriteResource](#6-writeresource)
7. [ListDevices](#7-listdevices)
8. [GetCommissionedDeviceInfo](#8-getcommissioneddeviceinfo)
9. [RemoveDevice](#9-removedevice)
10. [OpenCommissioningWindow](#10-opencommissioningwindow)

---

## 1. Constructor & Destructor

### BartonMatterImplementation()

**Layman's Explanation:**
Creates a new BartonMatter service instance. Like turning on your TV - everything gets powered up and ready to use.

**Technical Details:**
- Initializes member variables (bartonClient = nullptr)
- Sets up callback handlers for session establishment (mSuccessCallback, mFailureCallback)
- Logs creation with instance pointer for debugging

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Plugin Manager Creates Object  │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Constructor Invoked            │
│  - Initialize bartonClient=NULL │
│  - Setup success callback       │
│  - Setup failure callback       │
│  - Log instance address         │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Object Ready (Not Started)     │
└─────────────────────────────────┘
```

**Code Location:** Lines 40-46

---

### ~BartonMatterImplementation()

**Layman's Explanation:**
Shuts down everything gracefully when the service is stopped. Like turning off your TV - all connections are closed and memory is freed.

**Technical Details:**
- Shuts down cluster delegates (KeypadInput, ApplicationLauncher)
- Stops Barton Core client if running
- Releases GObject reference (calls g_object_unref)
- Frees network credential memory (g_free)
- Thread-safe cleanup with mutex lock

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Plugin Manager Destroys Object │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Destructor Invoked             │
│  - Shutdown cluster delegates   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Stop Barton Client             │
│  - b_core_client_stop()         │
│  - g_object_unref(bartonClient) │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Free Network Credentials       │
│  - Lock networkCredsMtx         │
│  - g_free(network_ssid)         │
│  - g_free(network_psk)          │
│  - Set pointers to NULL         │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Object Destroyed               │
└─────────────────────────────────┘
```

**Code Location:** Lines 48-65

**Memory Safety:** All heap allocations are properly freed, GObject refcounts decremented, mutexes released.

---

## 2. SetWifiCredentials

**Signature:**
```cpp
Core::hresult SetWifiCredentials(const std::string ssid, const std::string password)
```

**Layman's Explanation:**
Tells your TV the WiFi network name and password so it can help other devices (like Alexa casting apps) connect to the same network during setup. Like saving your WiFi info in your phone's settings.

**Technical Details:**
- **Input Validation:** Checks if SSID and password are non-empty
- **Storage:** Saves credentials in global C-style variables (network_ssid, network_psk)
- **Thread Safety:** Uses networkCredsMtx mutex for concurrent access protection
- **GLib Integration:** Uses g_strdup() for string duplication (memory managed by GLib)
- **Provider Pattern:** Credentials stored for later retrieval by BCoreNetworkCredentialsProvider

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  SetWifiCredentials(ssid, pass) │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Validate Input Parameters      │
│  - SSID empty? → ERROR          │
│  - Password empty? → ERROR      │
└──────────────┬──────────────────┘
               │ Valid
               ▼
┌─────────────────────────────────┐
│  Call GLib Credential Provider  │
│  b_reference_network_           │
│    credentials_provider_        │
│    set_wifi_network_credentials │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Store Credentials Globally     │
│  - Lock networkCredsMtx         │
│  - g_free(old values)           │
│  - network_ssid = g_strdup()    │
│  - network_psk = g_strdup()     │
│  - Unlock mutex                 │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Log Success                    │
│  Return Core::ERROR_NONE        │
└─────────────────────────────────┘
```

**Code Location:** Lines 195-212

**Usage Example:**
```cpp
// Set WiFi credentials before commissioning
result = bartonMatter->SetWifiCredentials("MyHomeWiFi", "SecurePassword123");
```

**Error Codes:**
- `Core::ERROR_NONE` (0): Success
- `Core::ERROR_INVALID_INPUT_LENGTH`: Empty SSID or password

**Thread Safety:** ✅ Yes - uses networkCredsMtx mutex

---

## 3. InitializeCommissioner

**Signature:**
```cpp
Core::hresult InitializeCommissioner()
```

**Layman's Explanation:**
Starts up the Matter service on your TV. Like pressing the power button - it wakes up all the smart home systems and gets ready to talk to other devices like Alexa.

**Technical Details:**
- **Configuration Directory:** Creates /opt/.brtn-ds/ with Matter storage subdirectory
- **Barton Client Initialization:** Creates BCoreClient instance with all configuration
- **Default Credentials:** Sets "MySSID"/"MyPassword" if not already configured
- **Signal Handlers:** Connects device added/removed/endpoint added event handlers
- **Cluster Delegates:** Schedules initialization on Matter event loop thread
- **System Properties:** Sets deviceDescriptorBypass=true to skip descriptor checks

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  InitializeCommissioner()       │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Check If Credentials Set       │
│  - Lock networkCredsMtx         │
│  - If NULL → Set defaults       │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Get Config Directory           │
│  GetConfigDirectory()           │
│  → "/opt/.brtn-ds/"             │
│  - Create if doesn't exist      │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  InitializeClient()             │
│  - Create params container      │
│  - Set storage directories      │
│  - Create network provider      │
│  - Create BCoreClient           │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Configure Barton Properties    │
│  SetDefaultParameters()         │
│  - Vendor ID: 0xFFF1            │
│  - Product ID: 0x5678           │
│  - Discriminator: 3840          │
│  - Passcode: 20202021           │
│  - Disable Thread/Zigbee        │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Connect Signal Handlers        │
│  - DeviceAddedHandler           │
│  - DeviceRemovedHandler         │
│  - EndpointAddedHandler         │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Start Barton Client            │
│  b_core_client_start()          │
│  - Initializes Matter stack     │
│  - Opens UDP ports              │
│  - Loads device database        │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Set System Properties          │
│  deviceDescriptorBypass=true    │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Initialize Cluster Delegates   │
│  Schedule on Matter Event Loop: │
│  - KeypadInput delegate         │
│  - ApplicationLauncher delegate │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Commissioner Ready             │
│  Return Core::ERROR_NONE        │
└─────────────────────────────────┘
```

**Code Location:** Lines 401-443

**Directory Structure Created:**
```
/opt/.brtn-ds/
├── matter/                    # Matter SDK storage
│   ├── chip_counters.ini
│   ├── chip_config.ini
│   └── ...
└── storage/
    └── devicedb/              # Device database files
        ├── 90034FD9068DFF14   # Node ID files
        └── ...
```

**Default Device Information:**
- Vendor Name: "Barton"
- Vendor ID: 0xFFF1 (CSA test vendor)
- Product Name: "Barton Device"
- Product ID: 0x5678
- Hardware Version: 1
- Discriminator: 3840 (well-known dev value)
- Passcode: 20202021 (well-known dev value)

**Thread Safety:** ✅ Yes - uses Platform::ScheduleWork for Matter thread, mutex for credentials

**Error Codes:**
- `Core::ERROR_NONE` (0): Success
- `Core::ERROR_GENERAL`: Failed to start Barton client

---

## 4. CommissionDevice

**Signature:**
```cpp
Core::hresult CommissionDevice(const std::string passcode)
```

**Layman's Explanation:**
Pairs a casting device (like your phone running an app) with your TV using a code. Like entering a pairing code to connect Bluetooth headphones, but for Matter smart home devices.

**Technical Details:**
- **Input:** Matter setup payload (QR code string or manual pairing code)
- **Timeout:** Hardcoded 120 seconds for commissioning window
- **Barton API:** Calls b_core_client_commission_device()
- **Async Process:** Returns immediately, actual commissioning happens in background
- **Event Flow:** DeviceAddedHandler fires on success
- **Commissioning Steps** (internal to Barton):
  1. PASE (Password Authenticated Session Establishment)
  2. Device attestation verification
  3. Operational credential installation
  4. Network credential provisioning (WiFi)
  5. Device configuration completion

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  CommissionDevice(passcode)     │
│  passcode = QR code or manual   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Validate Inputs                │
│  - bartonClient NULL? → ERROR   │
│  - passcode empty? → ERROR      │
└──────────────┬──────────────────┘
               │ Valid
               ▼
┌─────────────────────────────────┐
│  Call Commission()              │
│  - setupPayload = passcode      │
│  - timeout = 120 seconds        │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Barton Core Commission         │
│  b_core_client_commission_      │
│  device(client, payload, 120)   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Return Immediately             │
│  (Commissioning runs in bg)     │
│  Core::ERROR_NONE or ERROR      │
└──────────────┬──────────────────┘
               │
               │ (Meanwhile, in background thread...)
               │
               ▼
┌─────────────────────────────────┐
│  PASE: Password Exchange        │
│  - Derive PAKE key from passcode│
│  - Establish secure session     │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Device Attestation             │
│  - Verify DAC certificate chain │
│  - Check product authenticity   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Operational Credential Install │
│  - Generate NOC for device      │
│  - Device joins fabric          │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Network Credential Provisioning│
│  - Send WiFi SSID/password      │
│  - Device connects to WiFi      │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Configuration Discovery        │
│  - Read device descriptor       │
│  - Discover endpoints/clusters  │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  DeviceAddedHandler Fires       │
│  - Creates ACL entry            │
│  - Establishes CASE session     │
│  - Writes bindings              │
│  - Updates device cache         │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Commissioning Complete         │
│  Device operational on fabric   │
└─────────────────────────────────┘
```

**Code Location:** Lines 213-228

**Usage Example:**
```cpp
// Commission using QR code
result = bartonMatter->CommissionDevice("MT:Y.K9042C00KA0648G00");

// Commission using manual pairing code
result = bartonMatter->CommissionDevice("34970112332");
```

**Commissioning Flow Details:**

1. **PASE (0-10 seconds):**
   - Password-based session establishment
   - SPAKE2+ key exchange using passcode
   - Creates encrypted communication channel

2. **Attestation (10-20 seconds):**
   - Verify Device Attestation Certificate (DAC)
   - Check certificate signed by trusted PAA
   - Validate product is authentic Matter device

3. **Operational Credential (20-40 seconds):**
   - Generate Node Operational Certificate (NOC)
   - Install NOC on device
   - Device receives node ID and fabric ID

4. **Network Provisioning (40-80 seconds):**
   - Send WiFi credentials to device
   - Device disconnects from BLE/SoftAP
   - Device connects to WiFi network
   - Device discovers operational IP

5. **Configuration (80-120 seconds):**
   - Read device descriptor cluster
   - Discover all endpoints and clusters
   - Save to device database
   - Fire DeviceAddedHandler event

**Error Codes:**
- `Core::ERROR_NONE` (0): Commission request submitted successfully
- `Core::ERROR_GENERAL`: Barton client error or invalid passcode
- `Core::ERROR_INVALID_INPUT_LENGTH`: Empty passcode

**Timeout Behavior:**
- If commissioning doesn't complete in 120 seconds, FailSafe timer expires
- Device reverts to uncommissioned state
- Must retry commissioning from scratch

---

## 5. ReadResource

**Signature:**
```cpp
Core::hresult ReadResource(std::string uri, std::string resourceType, std::string& result)
```

**Layman's Explanation:**
Asks a connected device for information, like checking if a light is on/off or reading the current temperature. You tell it which device (uri) and what info you want (resourceType), and it gives you the answer.

**Technical Details:**
- **URI Construction:** Builds full path `/uri/ep/1/r/resourceType`
- **Barton API:** Calls b_core_client_read_resource()
- **Thread Safety:** GLib autoptr handles memory (g_autofree, g_autoptr)
- **Return Format:** String representation of attribute value
- **Endpoint Hardcoded:** Always reads from endpoint 1

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  ReadResource(uri, type, result)│
│  Example: uri="ABC123"          │
│           type="onOff"          │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Construct Full URI             │
│  fullUri = "/" + uri            │
│          + "/ep/1/r/"           │
│          + resourceType         │
│  Result: "/ABC123/ep/1/r/onOff" │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Call Barton Core API           │
│  b_core_client_read_resource()  │
│  - Resolves URI to node/cluster │
│  - Sends Matter Read Request    │
│  - Waits for response           │
└──────────────┬──────────────────┘
               │
               ▼
        ┌──────┴──────┐
        │             │
    Success         Error
        │             │
        ▼             ▼
┌─────────────┐ ┌─────────────┐
│ value!=NULL │ │ value==NULL │
│ err==NULL   │ │ or err!=NULL│
└──────┬──────┘ └──────┬──────┘
       │               │
       ▼               ▼
┌─────────────┐ ┌─────────────┐
│ Set result  │ │ Set result  │
│ to value    │ │ to ""       │
│ Log success │ │ Log error   │
│ Return NONE │ │ Return ERROR│
└─────────────┘ └─────────────┘
```

**Code Location:** Lines 229-248

**Usage Examples:**
```cpp
std::string value;

// Read light state (On/Off cluster)
result = bartonMatter->ReadResource("90034FD9068DFF14", "onOff", value);
// value = "true" or "false"

// Read brightness level (Level Control cluster)
result = bartonMatter->ReadResource("90034FD9068DFF14", "currentLevel", value);
// value = "128" (0-255)

// Read feature map
result = bartonMatter->ReadResource("90034FD9068DFF14", "featureMap", value);
// value = "1" (hex representation)
```

**URI Format:**
```
/{deviceUuid}/ep/{endpointId}/r/{attributeName}
```

**Matter Protocol Flow:**
1. Barton resolves URI to Matter node ID
2. Creates InteractionModelEngine Read request
3. Builds AttributePathIB (endpoint, cluster, attribute)
4. Sends ReadRequest over CASE session
5. Receives ReportDataMessage
6. Decodes AttributeDataIB
7. Converts value to string
8. Returns to caller

**Error Codes:**
- `Core::ERROR_NONE` (0): Read successful, result contains value
- `Core::ERROR_GENERAL`: Read failed (device offline, invalid URI, cluster not supported)

**Limitations:**
- Endpoint hardcoded to 1 (should be parameterized)
- Synchronous blocking call (can timeout)
- No type conversion - returns raw string

---

## 6. WriteResource

**Signature:**
```cpp
Core::hresult WriteResource(std::string uri, std::string resourceType, std::string value)
```

**Layman's Explanation:**
Sends a command to a device, like turning on a light or setting the volume. You tell it which device (uri), what to change (resourceType), and the new value.

**Technical Details:**
- **URI Construction:** Builds full path `/uri/ep/1/r/resourceType`
- **Barton API:** Calls b_core_client_write_resource()
- **Value Format:** String representation, Barton handles type conversion
- **Thread Safety:** GLib autoptr for error handling
- **Endpoint Hardcoded:** Always writes to endpoint 1

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  WriteResource(uri, type, value)│
│  Example: uri="ABC123"          │
│           type="onOff"          │
│           value="true"          │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Construct Full URI             │
│  fullUri = "/" + uri            │
│          + "/ep/1/r/"           │
│          + resourceType         │
│  Result: "/ABC123/ep/1/r/onOff" │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Log Write Attempt              │
│  "Writing onOff with value:true"│
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Call Barton Core API           │
│  b_core_client_write_resource() │
│  - Resolves URI to node/cluster │
│  - Converts string to type      │
│  - Sends Matter Write Request   │
│  - Waits for WriteResponse      │
└──────────────┬──────────────────┘
               │
               ▼
        ┌──────┴──────┐
        │             │
    Success         Error
        │             │
        ▼             ▼
┌─────────────┐ ┌─────────────┐
│ No error    │ │ err!=NULL   │
│ Log success │ │ Log error   │
│ Return NONE │ │ Return ERROR│
└─────────────┘ └─────────────┘
```

**Code Location:** Lines 250-274

**Usage Examples:**
```cpp
// Turn on a light
result = bartonMatter->WriteResource("90034FD9068DFF14", "onOff", "true");

// Set brightness to 50%
result = bartonMatter->WriteResource("90034FD9068DFF14", "currentLevel", "128");

// Set color temperature
result = bartonMatter->WriteResource("90034FD9068DFF14", "colorTemperatureMireds", "250");
```

**Supported Value Formats:**
- Boolean: "true" or "false"
- Integer: "123", "-456"
- Hex: "0x1A"
- String: "HelloWorld"
- Float: "3.14"

**Matter Protocol Flow:**
1. Barton resolves URI to Matter node ID
2. Parses value string to appropriate type
3. Creates InteractionModelEngine Write request
4. Builds AttributeDataIB (path + encoded value)
5. Sends WriteRequest over CASE session
6. Receives WriteResponse with status codes
7. Returns success/failure

**Error Codes:**
- `Core::ERROR_NONE` (0): Write successful
- `Core::ERROR_GENERAL`: Write failed (device offline, invalid value, read-only attribute)

**Limitations:**
- Endpoint hardcoded to 1
- Synchronous blocking call
- No validation of value format before sending

---

## 7. ListDevices

**Signature:**
```cpp
Core::hresult ListDevices(std::string& deviceList)
```

**Layman's Explanation:**
Shows you all the devices currently connected to your TV, like seeing a list of all Bluetooth devices paired with your phone. Returns device IDs in a simple list format.

**Technical Details:**
- **Barton API:** Calls b_core_client_get_devices()
- **GLib Autolist:** Uses g_autolist for automatic memory management
- **Return Format:** JSON array of device UUID strings
- **Device UUID:** Matter node ID in hex format (e.g., "90034FD9068DFF14")

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  ListDevices(deviceList)        │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Check Barton Client            │
│  bartonClient == NULL?          │
│  → Return [] + ERROR_UNAVAILABLE│
└──────────────┬──────────────────┘
               │ Client exists
               ▼
┌─────────────────────────────────┐
│  Get All Connected Devices      │
│  deviceObjects =                │
│  b_core_client_get_devices()    │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Check Device List              │
│  deviceObjects == NULL?         │
│  → Return [] + ERROR_UNAVAILABLE│
└──────────────┬──────────────────┘
               │ Has devices
               ▼
┌─────────────────────────────────┐
│  Iterate Through Devices        │
│  for each device in list:       │
│  ┌──────────────────────────┐   │
│  │ Get UUID property        │   │
│  │ g_object_get(device,     │   │
│  │   "uuid", &deviceId)     │   │
│  └──────────────────────────┘   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Add Valid UUIDs to Vector      │
│  if (deviceId != NULL):         │
│    deviceUuids.push_back()      │
│    Log device found             │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Check If Any Valid Devices     │
│  deviceUuids.empty()?           │
│  → Return [] + ERROR_UNAVAILABLE│
└──────────────┬──────────────────┘
               │ Has UUIDs
               ▼
┌─────────────────────────────────┐
│  Build JSON Array               │
│  deviceList = "["               │
│  for each uuid:                 │
│    append "\"" + uuid + "\""    │
│    append "," (if not last)     │
│  append "]"                     │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Return Success                 │
│  Log total device count         │
│  Return Core::ERROR_NONE        │
└─────────────────────────────────┘
```

**Code Location:** Lines 1058-1096

**Usage Example:**
```cpp
std::string deviceList;
result = bartonMatter->ListDevices(deviceList);

// Success: deviceList = ["90034FD9068DFF14", "A1B2C3D4E5F60718"]
// No devices: deviceList = "[]"
```

**Return Format:**
```json
[
  "90034FD9068DFF14",
  "A1B2C3D4E5F60718",
  "FEDCBA9876543210"
]
```

**Device UUID Format:**
- 16-character hex string
- Represents 64-bit Matter node ID
- Unique within a fabric
- Assigned during commissioning

**Error Codes:**
- `Core::ERROR_NONE` (0): Success, deviceList contains JSON array
- `Core::ERROR_UNAVAILABLE`: Barton client not initialized or no devices found

**Thread Safety:** ✅ Yes - Barton Core handles internal locking

---

## 8. GetCommissionedDeviceInfo

**Signature:**
```cpp
Core::hresult GetCommissionedDeviceInfo(std::string& deviceInfo)
```

**Layman's Explanation:**
Shows detailed information about all devices that have been set up with your TV, including what type of device it is (like "TV-CASTING" for a casting app). Like viewing your device list in the Alexa app with names and types.

**Technical Details:**
- **Cache Management:** Uses in-memory map (commissionedDevicesCache)
- **Lazy Initialization:** Scans devicedb directory on first call
- **File Parsing:** Reads device files to extract model names
- **Return Format:** JSON array with nodeId and model for each device
- **Thread Safety:** Uses devicesCacheMtx mutex

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  GetCommissionedDeviceInfo(info)│
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Lock Device Cache Mutex        │
│  std::lock_guard<std::mutex>    │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Check If Cache Initialized     │
│  devicesCacheInitialized?       │
└──────────────┬──────────────────┘
               │ Not initialized
               ▼
┌─────────────────────────────────┐
│  Scan Device Database           │
│  ScanDeviceDatabase()           │
│  ┌──────────────────────────┐   │
│  │ Open /opt/.brtn-ds/      │   │
│  │   storage/devicedb/      │   │
│  │ For each file:           │   │
│  │   - Skip .bak, system    │   │
│  │   - Extract model name   │   │
│  │   - Add to cache         │   │
│  └──────────────────────────┘   │
│  Set devicesCacheInitialized    │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Check Cache Contents           │
│  commissionedDevicesCache       │
│    .empty()?                    │
│  → Return [] + ERROR_UNAVAILABLE│
└──────────────┬──────────────────┘
               │ Has devices
               ▼
┌─────────────────────────────────┐
│  Build JSON Array               │
│  deviceInfo = "["               │
│  for each (nodeId, model):      │
│  ┌──────────────────────────┐   │
│  │ Append "{"               │   │
│  │ "nodeId":"90034F..."     │   │
│  │ "model":"TV-CASTING"     │   │
│  │ "}"                      │   │
│  │ "," (if not last)        │   │
│  └──────────────────────────┘   │
│  Append "]"                     │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Unlock Mutex & Return          │
│  Log device count               │
│  Return Core::ERROR_NONE        │
└─────────────────────────────────┘
```

**Code Location:** Lines 1098-1124

**Cache Management Flow:**
```
First Call:
  → devicesCacheInitialized = false
  → ScanDeviceDatabase()
  → Populate commissionedDevicesCache
  → Set devicesCacheInitialized = true
  → Return data

Subsequent Calls:
  → devicesCacheInitialized = true
  → Skip scan
  → Return cached data immediately

Cache Invalidation:
  → RemoveDevice() clears cache
  → Next GetCommissionedDeviceInfo() rescans
```

**Device Database Structure:**
```
/opt/.brtn-ds/storage/devicedb/
├── 90034FD9068DFF14           # Device file (node ID as filename)
│   Contains JSON: {"model": {"value": "TV-CASTING"}, ...}
├── A1B2C3D4E5F60718
└── system.properties
```

**File Parsing Logic (ExtractModelFromDeviceFile):**
1. Read entire file as binary string
2. Search for `"model"` key
3. Find `"value"` field within model object
4. Extract string between quotes
5. Return model name (e.g., "TV-CASTING")

**Usage Example:**
```cpp
std::string deviceInfo;
result = bartonMatter->GetCommissionedDeviceInfo(deviceInfo);

// Result:
// [
//   {"nodeId":"90034FD9068DFF14","model":"TV-CASTING"},
//   {"nodeId":"A1B2C3D4E5F60718","model":"LIGHT-BULB"}
// ]
```

**Error Codes:**
- `Core::ERROR_NONE` (0): Success, deviceInfo contains JSON array
- `Core::ERROR_UNAVAILABLE`: No commissioned devices found

**Thread Safety:** ✅ Yes - uses devicesCacheMtx mutex for all cache operations

**Cache Update Events:**
- **Add:** DeviceAddedHandler() calls UpdateDeviceCache()
- **Remove:** DeviceRemovedHandler() calls RemoveDeviceFromCache()
- **Clear:** RemoveDevice() sets devicesCacheInitialized = false

---

## 9. RemoveDevice

**Signature:**
```cpp
Core::hresult RemoveDevice(const std::string deviceUuid)
```

**Layman's Explanation:**
Unpairs a device from your TV, like removing a Bluetooth device from your phone. The device forgets your TV and your TV forgets the device - you'll need to pair again to reconnect.

**Technical Details:**
- **Barton API:** Calls b_core_client_remove_device()
- **Fabric Removal:** Deletes device from Matter fabric
- **File Deletion:** Removes devicedb file (/opt/.brtn-ds/storage/devicedb/{nodeId})
- **Cache Invalidation:** Clears commissionedDevicesCache and forces rescan
- **ACL Cleanup:** Barton automatically removes ACL entries for the device

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  RemoveDevice(deviceUuid)       │
│  Example: "90034FD9068DFF14"    │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Validate Inputs                │
│  - bartonClient NULL? → ERROR   │
│  - deviceUuid empty? → ERROR    │
└──────────────┬──────────────────┘
               │ Valid
               ▼
┌─────────────────────────────────┐
│  Call Barton Core API           │
│  result = b_core_client_remove_ │
│           device(client, uuid)  │
└──────────────┬──────────────────┘
               │
               ▼
        ┌──────┴──────┐
        │             │
    Success         Error
        │             │
        ▼             ▼
┌─────────────┐ ┌─────────────┐
│ result=TRUE │ │ result=FALSE│
└──────┬──────┘ └──────┬──────┘
       │               │
       │               ▼
       │         ┌─────────────┐
       │         │ Log error   │
       │         │ Return ERROR│
       │         └─────────────┘
       │
       ▼
┌─────────────────────────────────┐
│  Barton Internal Actions        │
│  (Automatic)                    │
│  ┌──────────────────────────┐   │
│  │ Delete devicedb file     │   │
│  │ Remove from fabric       │   │
│  │ Delete ACL entries       │   │
│  │ Fire DeviceRemovedEvent  │   │
│  └──────────────────────────┘   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Clear Device Cache             │
│  Lock devicesCacheMtx           │
│  commissionedDevicesCache.clear()│
│  devicesCacheInitialized=false  │
│  Unlock mutex                   │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  DeviceRemovedHandler Fires     │
│  (Signal from Barton)           │
│  RemoveDeviceFromCache(nodeId)  │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Log Success & Return           │
│  "Successfully removed device"  │
│  Return Core::ERROR_NONE        │
└─────────────────────────────────┘
```

**Code Location:** Lines 1126-1156

**What Gets Deleted:**

1. **Device Database File:**
   ```
   /opt/.brtn-ds/storage/devicedb/90034FD9068DFF14
   ```

2. **ACL Entries:**
   - All access control entries granting this device access to TV endpoints
   - Prevents removed device from sending commands

3. **Fabric Association:**
   - Device is removed from Matter fabric
   - Device's operational certificate invalidated
   - Device reverts to uncommissioned state

4. **In-Memory Cache:**
   - Entry removed from commissionedDevicesCache
   - devicesCacheInitialized reset to false
   - Next GetCommissionedDeviceInfo() will rescan

**Usage Example:**
```cpp
// Remove a casting device
result = bartonMatter->RemoveDevice("90034FD9068DFF14");

// Result: Device unpaired, can no longer communicate with TV
```

**Side Effects:**
- Device cannot communicate with TV anymore
- Device must be recommissioned to reconnect
- Device shows as "unpaired" in its app
- TV's device list no longer shows this device

**Error Codes:**
- `Core::ERROR_NONE` (0): Device removed successfully
- `Core::ERROR_UNAVAILABLE`: Barton client not initialized
- `Core::ERROR_INVALID_INPUT_LENGTH`: Empty deviceUuid
- `Core::ERROR_GENERAL`: Device not found or removal failed

**Thread Safety:** ✅ Yes - uses devicesCacheMtx mutex, Barton Core handles internal locking

**Cache Synchronization:**
- Clearing cache ensures consistency with filesystem
- Forced rescan on next read prevents stale data
- DeviceRemovedHandler provides double protection

---

## 10. OpenCommissioningWindow

**Signature:**
```cpp
Core::hresult OpenCommissioningWindow(const uint16_t timeoutSeconds, std::string& commissioningInfo)
```

**Layman's Explanation:**
Makes your TV discoverable so Alexa or other smart home controllers can connect to it. Like putting your Bluetooth device in "pairing mode" - it generates codes that Alexa scans to connect. The window stays open for the time you specify, then automatically closes for security.

**Technical Details:**
- **Barton API:** Calls b_core_client_open_commissioning_window()
- **Target Device:** Device ID "0" means local device (this TV)
- **Return Values:**
  - Manual Code: 11-digit decimal pairing code
  - QR Code: Matter QR code string (MT:Y.K9042C00KA0648G00 format)
- **Timeout:** Default timeout used if parameter is 0
- **Security:** Window auto-closes after timeout to prevent unauthorized pairing

**Flow Diagram:**
```
┌─────────────────────────────────┐
│  Thunder API Call               │
│  OpenCommissioningWindow(       │
│    timeoutSeconds,              │
│    commissioningInfo            │
│  )                              │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Validate Barton Client         │
│  bartonClient == NULL?          │
│  → Return {} + ERROR_UNAVAILABLE│
└──────────────┬──────────────────┘
               │ Client exists
               ▼
┌─────────────────────────────────┐
│  Call Barton Core API           │
│  info = b_core_client_open_     │
│    commissioning_window(        │
│      client,                    │
│      "0",          // local dev │
│      timeoutSeconds             │
│    )                            │
└──────────────┬──────────────────┘
               │
               ▼
        ┌──────┴──────┐
        │             │
    Success         Error
        │             │
        ▼             ▼
  ┌─────────┐   ┌─────────┐
  │info!=NULL│   │info==NULL│
  └────┬────┘   └────┬────┘
       │             │
       │             ▼
       │       ┌─────────────┐
       │       │ Return {}   │
       │       │ + ERROR     │
       │       └─────────────┘
       │
       ▼
┌─────────────────────────────────┐
│  Extract Commissioning Codes    │
│  g_object_get(info,             │
│    "manual-code", &manualCode,  │
│    "qr-code", &qrCode,          │
│    NULL                         │
│  )                              │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Validate Extracted Codes       │
│  manualCode==NULL || qrCode==NULL│
│  → Return {} + ERROR_GENERAL    │
└──────────────┬──────────────────┘
               │ Valid codes
               ▼
┌─────────────────────────────────┐
│  Build JSON Response            │
│  commissioningInfo = "{"        │
│  "manualCode":"34970112332"     │
│  ","                            │
│  "qrCode":"MT:Y.K9042C00..."    │
│  "}"                            │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Log Success                    │
│  "Commissioning window opened"  │
│  "Manual Code: 34970112332"     │
│  "QR Code: MT:Y.K9042C00..."    │
└──────────────┬──────────────────┘
               │
               ▼
┌─────────────────────────────────┐
│  Return Success                 │
│  Return Core::ERROR_NONE        │
└─────────────────────────────────┘
```

**Code Location:** Lines 1288-1336

**Matter Protocol Flow (Internal to Barton):**

1. **Generate Random Passcode:**
   - 27-bit random value
   - Converted to 11-digit decimal
   - Used for PASE session

2. **Open BLE/mDNS Advertising:**
   - BLE: Start advertising with discriminator
   - mDNS: Publish _matterc._udp service
   - Include device identifiers in TXT records

3. **Generate QR Code:**
   - Encode: Version | VID | PID | Discriminator | Passcode
   - Format: Base38 encoded string
   - Prefix: "MT:" (Matter standard)

4. **Start Timeout Timer:**
   - Create FailSafe timer with specified duration
   - Timer expires → close commissioning window
   - Prevents indefinite exposure

5. **Wait for Commissioner:**
   - Accept incoming PASE requests
   - Validate passcode/discriminator
   - Proceed with commissioning if valid

**Usage Example:**
```cpp
std::string commissioningInfo;
uint16_t timeout = 900; // 15 minutes

result = bartonMatter->OpenCommissioningWindow(timeout, commissioningInfo);

// Success result:
// {
//   "manualCode": "34970112332",
//   "qrCode": "MT:Y.K9042C00KA0648G00"
// }
```

**Return Format:**
```json
{
  "manualCode": "34970112332",
  "qrCode": "MT:Y.K9042C00KA0648G00"
}
```

**Commissioning Code Details:**

**Manual Code (11 digits):**
- Format: DDDDDDDDDDD (11 decimal digits)
- Example: 34970112332
- Contains: Discriminator (12 bits) + Passcode (27 bits)
- Usage: User types into controller manually
- Checksum: Last digit is Verhoeff checksum

**QR Code:**
- Format: MT:Y.K9042C00KA0648G00
- Encoding: Base38 (0-9, A-Z, minus I/O/Q/Z)
- Contains:
  - Version: 0
  - Vendor ID: 0xFFF1
  - Product ID: 0x5678
  - Discovery Capabilities: 0x02 (BLE)
  - Discriminator: 3840
  - Passcode: 20202021
- Usage: Scan with phone camera in Alexa app

**Security Considerations:**

1. **Time-Limited:** Window closes after timeout
2. **One-Time Codes:** New codes generated each time
3. **Rate Limiting:** Prevents brute force attacks
4. **FailSafe:** Aborts commissioning if timeout expires
5. **Secure Session:** PASE uses SPAKE2+ key exchange

**Timeout Values:**
- Minimum: 180 seconds (3 minutes)
- Maximum: 900 seconds (15 minutes)
- Default: 300 seconds (5 minutes) if 0 passed
- Recommendation: 300-600 seconds for user convenience

**Error Codes:**
- `Core::ERROR_NONE` (0): Window opened successfully
- `Core::ERROR_UNAVAILABLE`: Barton client not initialized
- `Core::ERROR_GENERAL`: Failed to open window or extract codes

**Thread Safety:** ✅ Yes - Barton Core handles thread-safe window management

**Typical Alexa Flow:**
1. User: "Alexa, discover devices"
2. TV: OpenCommissioningWindow(600) → returns QR code
3. TV UI: Shows QR code on screen
4. User: Opens Alexa app, scans QR code
5. Alexa: Extracts passcode/discriminator, starts commissioning
6. TV: Accepts PASE request, completes commissioning
7. Window closes automatically after 600 seconds or successful commission

---

## Appendix: Complete Commissioning Flow

Here's how all these APIs work together in a typical setup:

```
┌─────────────────────────────────────────────────────────────┐
│                  COMPLETE COMMISSIONING FLOW                │
└─────────────────────────────────────────────────────────────┘

1. INITIALIZATION (TV Boot)
   ┌─────────────────────────┐
   │ BartonMatterImplementation()  │  Constructor
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ SetWifiCredentials()    │  Store network credentials
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ InitializeCommissioner()│  Start Matter stack
   └────────────┬────────────┘
                │
                └──→ TV ready for commissioning

2. TV COMMISSIONING BY ALEXA
   ┌─────────────────────────┐
   │ OpenCommissioningWindow()│  User: "Alexa, discover"
   └────────────┬────────────┘
                │
                ├──→ Display QR code on TV
                │
                ▼
   ┌─────────────────────────┐
   │ Alexa scans QR code     │  Extracts passcode
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ Alexa commissions TV    │  PASE + Credentials
   └────────────┬────────────┘
                │
                └──→ TV paired with Alexa

3. CASTING APP COMMISSIONING (by TV)
   ┌─────────────────────────┐
   │ Casting app displays QR │  App in pairing mode
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ CommissionDevice()      │  TV scans app's QR
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ DeviceAddedHandler()    │  Automatic ACL creation
   └────────────┬────────────┘
                │
                ├──→ ConfigureClientACL()
                ├──→ EstablishSession()
                └──→ WriteClientBindings()

4. DEVICE INTERACTION
   ┌─────────────────────────┐
   │ ReadResource()          │  Check device state
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ WriteResource()         │  Send commands
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ ListDevices()           │  Show connected devices
   └────────────┬────────────┘
                │
                ▼
   ┌─────────────────────────┐
   │ GetCommissionedDeviceInfo() │  Show device details
   └─────────────────────────┘

5. DEVICE REMOVAL
   ┌─────────────────────────┐
   │ RemoveDevice()          │  Unpair device
   └────────────┬────────────┘
                │
                ├──→ Delete devicedb file
                ├──→ Remove ACL entries
                └──→ Clear cache

6. SHUTDOWN
   ┌─────────────────────────┐
   │ ~BartonMatterImplementation() │  TV power off
   └────────────┬────────────┘
                │
                ├──→ Shutdown delegates
                ├──→ Stop Barton client
                └──→ Free memory
```

---

## Appendix: Error Code Reference

| Error Code | Constant | Meaning | Typical Cause |
|------------|----------|---------|---------------|
| 0 | Core::ERROR_NONE | Success | Operation completed |
| 1 | Core::ERROR_GENERAL | Generic failure | Barton API error, network issue |
| 5 | Core::ERROR_UNAVAILABLE | Resource unavailable | Client not initialized, no devices |
| 22 | Core::ERROR_INVALID_INPUT_LENGTH | Input validation failed | Empty string parameter |

---

## Appendix: Thread Safety Summary

| API | Thread Safe | Protection Mechanism |
|-----|-------------|---------------------|
| Constructor | ✅ | No shared state modified |
| Destructor | ✅ | networkCredsMtx mutex |
| SetWifiCredentials | ✅ | networkCredsMtx mutex |
| InitializeCommissioner | ✅ | PlatformMgr::ScheduleWork |
| CommissionDevice | ✅ | Barton Core internal locking |
| ReadResource | ✅ | Barton Core internal locking |
| WriteResource | ✅ | Barton Core internal locking |
| ListDevices | ✅ | Barton Core internal locking |
| GetCommissionedDeviceInfo | ✅ | devicesCacheMtx mutex |
| RemoveDevice | ✅ | devicesCacheMtx + Barton locking |
| OpenCommissioningWindow | ✅ | Barton Core internal locking |

---

## Appendix: GLib Memory Management

BartonMatter uses GLib's automatic memory management:

```cpp
// Automatic cleanup when variables go out of scope:
g_autofree       // Calls g_free() on scope exit
g_autoptr        // Calls g_object_unref() on scope exit
g_autolist       // Frees GList on scope exit
```

**Example:**
```cpp
g_autofree gchar *value = b_core_client_read_resource(...);
// No need to call g_free(value) - automatic cleanup
```

---

## Appendix: Matter URI Format

Barton uses hierarchical URIs for resource addressing:

```
/{deviceUuid}/ep/{endpointId}/r/{attributeName}
```

**Examples:**
```
/90034FD9068DFF14/ep/1/r/onOff
/90034FD9068DFF14/ep/1/r/currentLevel
/90034FD9068DFF14/ep/2/r/currentVolume
```

**Components:**
- **deviceUuid:** 16-char hex Matter node ID
- **ep:** Endpoint ID (1-based)
- **r:** Resource (attribute)

---

## Document History

| Version | Date | Author | Changes |
|---------|------|--------|---------|
| 1.0 | 2025-12-24 | Copilot | Initial comprehensive documentation |

---

**End of BartonMatter API Reference**
