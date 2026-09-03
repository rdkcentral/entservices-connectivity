# Bluetooth SDK Integration Guide for Plugins

This document shows how to integrate the Bluetooth SDK stub into your Bluetooth plugin (btmgr or similar).

## Step 1: Add SDK Headers to Your Build

In your plugin's CMakeLists.txt:

```cmake
cmake_minimum_required(VERSION 3.10)
project(BluetoothPlugin)

set(CMAKE_CXX_STANDARD 17)

# Option 1: Build SDK as a subdirectory (recommended for tight integration)
add_subdirectory(../bluetooth-sdk-stub-integration-example sdk_build)

# Option 2: Or link against pre-built librdk_bluetooth.so
# find_library(RDK_BLUETOOTH rdk_bluetooth REQUIRED)

add_executable(btmgr
    src/main.cpp
    src/adapter_manager.cpp
    src/device_manager.cpp
)

# Include SDK headers
target_include_directories(btmgr PRIVATE
    ../bluetooth-sdk-stub-integration-example/include
)

# Link SDK library
target_link_libraries(btmgr PRIVATE
    rdk_bluetooth
    Threads::Threads
)
```

## Step 2: Use the SDK API in Your Code

### Basic Manager Setup

```cpp
#include <bluetooth/Manager.h>
#include <bluetooth/Adapter.h>
#include <bluetooth/Device.h>

using namespace bluetooth;

int main() {
    // Create manager
    Manager manager(AuthorisationMode::NoAuthorisation);
    
    // Get default adapter
    std::shared_ptr<Adapter> adapter;
    Status status = manager.getDefaultAdapter(adapter);
    if (!status) {
        std::cerr << "Failed to get adapter: " << status.get_message() << std::endl;
        return 1;
    }
    
    // Power on adapter
    status = adapter->setPowered(true);
    if (!status) {
        std::cerr << "Failed to power on: " << status.get_message() << std::endl;
    }
    
    return 0;
}
```

### Device Discovery and Connection

```cpp
#include <bluetooth/Adapter.h>
#include <bluetooth/Device.h>

// Start scanning
ScanFilter filter;
filter.type = ScanType::LeOnly;  // BLE only
filter.pattern = "";  // Match all devices

adapter->startScan(filter);

// Register for device discovery events
adapter->registerForEvents([](AdapterEvent event, AdapterEventData data) {
    if (event == AdapterEvent::DeviceDiscovered) {
        std::string addr;
        data.device->address(addr);
        std::cout << "Found device: " << addr << std::endl;
        
        // Connect to device
        Status s = data.device->connect();
    }
});

// Stop scanning after 30 seconds
std::this_thread::sleep_for(std::chrono::seconds(30));
adapter->stopScan();
```

### Device Properties

```cpp
#include <bluetooth/Device.h>

auto device = adapter->getDevice("00:11:22:33:44:55");
if (device) {
    std::string name;
    device->name(name);
    std::cout << "Device name: " << name << std::endl;
    
    std::string address;
    device->address(address);
    std::cout << "Address: " << address << std::endl;
    
    int8_t rssi;
    device->rssi(rssi);
    std::cout << "RSSI: " << (int)rssi << " dBm" << std::endl;
    
    bool connected;
    device->connected(connected);
    std::cout << "Connected: " << (connected ? "yes" : "no") << std::endl;
    
    // Get all properties at once
    DeviceProperties props;
    device->getAllProperties(props);
}
```

### GATT Client (Reading Characteristics)

```cpp
#include <bluetooth/Device.h>
#include <bluetooth/GattClient.h>

auto gatt_client = device->getGattClient();

// Get all services
auto services = gatt_client->getServices();
for (const auto& service : services) {
    std::string uuid;
    service->uuid(uuid);
    std::cout << "Service UUID: " << uuid << std::endl;
    
    // Get characteristics in this service
    auto chars = service->getCharacteristics();
    for (const auto& chr : chars) {
        std::string chr_uuid;
        chr->uuid(chr_uuid);
        std::cout << "  Characteristic UUID: " << chr_uuid << std::endl;
        
        // Read characteristic value
        std::vector<uint8_t> value;
        Status s = chr->readValue(value);
        if (s) {
            std::cout << "    Value: ";
            for (uint8_t b : value) std::cout << std::hex << (int)b << " ";
            std::cout << std::dec << std::endl;
        }
    }
}
```

### Audio Control (with AUDIO_SUPPORT=ON)

```cpp
#include <bluetooth/Device.h>

// Set volume
Status s = device->setVolume(0.75f);  // 75% volume

// Get volume
float vol = device->getVolume();
std::cout << "Volume: " << (vol * 100) << "%" << std::endl;

// Mute
device->setMute(true);
bool muted = device->isMuted();

// Set delay compensation (in milliseconds)
device->setDelayCompensation(50);  // 50ms delay

// Register for audio events
device->registerForAudioEvents([](AudioEvent event, AudioEventData data) {
    switch (event) {
        case AudioEvent::VolumeChanged:
            std::cout << "Volume changed to " << data.volume << std::endl;
            break;
        case AudioEvent::MuteStateChanged:
            std::cout << "Mute state: " << (data.muted ? "muted" : "unmuted") << std::endl;
            break;
        case AudioEvent::DelayCompensationChanged:
            std::cout << "Delay: " << data.delayCompensation << " ms" << std::endl;
            break;
    }
});
```

### Event Handling Pattern

```cpp
#include <bluetooth/Adapter.h>

adapter->registerForEvents([](AdapterEvent event, AdapterEventData data) {
    switch (event) {
        case AdapterEvent::DiscoveryStarted:
            std::cout << "Discovery started" << std::endl;
            break;
        case AdapterEvent::DiscoveryStopped:
            std::cout << "Discovery stopped" << std::endl;
            break;
        case AdapterEvent::PoweredOn:
            std::cout << "Adapter powered on" << std::endl;
            break;
        case AdapterEvent::PoweredOff:
            std::cout << "Adapter powered off" << std::endl;
            break;
        case AdapterEvent::DeviceDiscovered: {
            std::string addr;
            data.device->address(addr);
            std::cout << "Device discovered: " << addr << std::endl;
            break;
        }
        case AdapterEvent::DeviceDisappeared: {
            std::string addr;
            data.device->address(addr);
            std::cout << "Device disappeared: " << addr << std::endl;
            break;
        }
    }
});
```

## Step 3: Handle Stub vs. Real SDK

### Determine Implementation at Runtime

The stub returns `Status::BLUETOOTH_ERROR` for operations that require real hardware:

```cpp
bool is_stub = !adapter->setPowered(true);  // Will fail on stub

// More reliable check:
std::string addr;
if (device->address(addr) && !addr.empty()) {
    std::cout << "Real SDK: got real MAC address" << std::endl;
} else {
    std::cout << "Stub SDK: device address not available" << std::endl;
}
```

### Graceful Fallback

```cpp
Status status = device->connect();
if (!status) {
    if (status.get_message().find("stub") != std::string::npos) {
        std::cout << "Using stub SDK; skipping real device operation" << std::endl;
        // Gracefully handle stub mode
    } else {
        std::cerr << "Bluetooth error: " << status.get_message() << std::endl;
    }
}
```

## Step 4: Build and Test

### Build with Stub

```bash
cd /path/to/plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

### Build with Audio Support (stub only)

```bash
cd /path/to/plugin
cmake -B build -DCMAKE_BUILD_TYPE=Release \
    -DAUDIO_SUPPORT=ON
cmake --build build
```

## Step 5: Deployment

### For Development/Testing

Link against the stub; no runtime Bluetooth hardware or services required.

### For Production

- Products without SDK: Ship stub `.so` (built once, no dependencies).
- Products with SDK: Replace stub `.so` with real `librdk_bluetooth.so` at runtime.

Middleware code remains unchanged because the ABI is identical when both are built with the same `-DAUDIO_SUPPORT=` setting.

## Common Patterns

### Connection Flow

```cpp
std::shared_ptr<Device> device = ...;

// Pair if not paired
if (needs_pairing) {
    Status s = device->pair(true, 30);  // sync, 30s timeout
    if (!s) {
        std::cerr << "Pairing failed: " << s.get_message() << std::endl;
        return;
    }
}

// Connect
Status s = device->connect(true, 10);  // sync, 10s timeout
if (!s) {
    std::cerr << "Connect failed: " << s.get_message() << std::endl;
    return;
}

// Device is now connected
bool connected;
device->connected(connected);
```

### Enumerate All Devices

```cpp
auto adapters = manager.getAdapters();
for (const auto& adapter : adapters) {
    auto devices = adapter->getDevices();
    for (const auto& device : devices) {
        std::string addr;
        device->address(addr);
        std::cout << "Device: " << addr << std::endl;
    }
}
```

## Troubleshooting

| Issue | Cause | Solution |
|-------|-------|----------|
| Undefined reference to `bluetooth::*` | Missing SDK includes | Add `-I../bluetooth-sdk-stub-integration-example/include` |
| Undefined reference to `librdk_bluetooth` | Missing SDK library | Link `-lrdk_bluetooth` or add it to `target_link_libraries` |
| AUDIO_SUPPORT methods not found | Built without `-DAUDIO_SUPPORT=ON` | Rebuild with `-DAUDIO_SUPPORT=ON` |
| Status returns error on every call | Using stub SDK | Expected; stub returns errors for real device operations |

## See Also

- `README.md`: Detailed feature overview and deployment patterns.
- Parent `bluetooth-sdk/` repository: Real SDK implementation, build scripts, Yocto recipes.
