# Quick Start: Building the Bluetooth SDK Stub

## For the Bluetooth Plugin Team

### 1. Build the Stub Library

```bash
cd bluetooth-sdk-stub-integration-example
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DAUDIO_SUPPORT=ON
cmake --build . --parallel
```

Output: `build/librdk_bluetooth.so` (or `.dylib` on macOS)

### 2. Include in Your Project

**CMakeLists.txt:**
```cmake
add_subdirectory(../bluetooth-sdk-stub-integration-example sdk)

target_include_directories(your_target PRIVATE
    ../bluetooth-sdk-stub-integration-example/include)

target_link_libraries(your_target PRIVATE rdk_bluetooth)
```

**Or, if pre-built:**
```cmake
target_include_directories(your_target PRIVATE /path/to/sdk/include)
target_link_libraries(your_target PRIVATE -L/path/to/sdk/build -lrdk_bluetooth)
```

### 3. Use the API

```cpp
#include <bluetooth/Manager.h>
#include <bluetooth/Adapter.h>

bluetooth::Manager manager;
std::shared_ptr<bluetooth::Adapter> adapter;
manager.getDefaultAdapter(adapter);
adapter->setPowered(true);
```

### 4. Stub Behavior

- All hardware operations return `Status::BLUETOOTH_ERROR`.
- State is tracked locally (useful for testing state machines).
- Audio methods are available when built with `-DAUDIO_SUPPORT=ON`.
- **No PipeWire, WirePlumber, BlueZ, or D-Bus required.**

### 5. Switch to Real SDK (Optional)

To use the real Bluetooth SDK instead:

1. Build the real SDK with: `cmake -S . -B build -DAUDIO_SUPPORT=ON -DBUILD_STUB_SDK=OFF`
2. Replace `librdk_bluetooth.so` at runtime.
3. Your middleware code doesn't change—the ABI is identical.

## Minimal Example

```cpp
#include <iostream>
#include <bluetooth/Manager.h>
#include <bluetooth/Adapter.h>

int main() {
    using namespace bluetooth;
    
    // Create a manager
    Manager mgr;
    
    // Get the adapter
    std::shared_ptr<Adapter> adapter;
    Status s = mgr.getDefaultAdapter(adapter);
    
    if (s) {
        std::cout << "Got adapter, powering on..." << std::endl;
        adapter->setPowered(true);
        
        std::cout << "Starting scan..." << std::endl;
        adapter->startScan();
        
        std::cout << "Devices found:" << std::endl;
        auto devices = adapter->getDevices();
        for (const auto& dev : devices) {
            std::string addr;
            dev->address(addr);
            std::cout << "  " << addr << std::endl;
        }
        
        adapter->stopScan();
    } else {
        std::cerr << "Error: " << s.get_message() << std::endl;
    }
    
    return 0;
}
```

## Build Options

| Option | Value | Effect |
|--------|-------|--------|
| `AUDIO_SUPPORT` | `ON` or `OFF` | Include audio method symbols (no linking overhead) |
| `CMAKE_BUILD_TYPE` | `Release` or `Debug` | Optimization level |

## File Structure

```
bluetooth-sdk-stub-integration-example/
├── CMakeLists.txt          ← Build configuration
├── README.md               ← Full documentation
├── INTEGRATION.md          ← Integration examples
├── QUICKSTART.md           ← This file
├── include/
│   ├── *.h                 ← Support headers
│   └── bluetooth/
│       ├── Manager.h       ← Main entry point
│       ├── Adapter.h       ← Adapter operations
│       ├── Device.h        ← Device operations
│       ├── Audio.h         ← Audio control (when AUDIO_SUPPORT=ON)
│       ├── GattClient.h    ← GATT client
│       ├── GattServer.h    ← GATT server
│       └── ...
└── stub/
    ├── Manager.cpp         ← Stub implementations
    ├── Adapter.cpp
    ├── Device.cpp
    ├── Audio.cpp           ← Audio stub
    └── ...
```

## FAQ

**Q: Do I need BlueZ?**  
A: No. The stub is completely independent.

**Q: Do I need PipeWire or WirePlumber?**  
A: No, even with `-DAUDIO_SUPPORT=ON`. Audio method signatures are available, but the stub doesn't call them.

**Q: Can I switch between stub and real SDK without recompiling my code?**  
A: Yes! Just replace the `.so` file at runtime. The ABI is identical when both are built with the same `-DAUDIO_SUPPORT=` setting.

**Q: Why does every call return an error?**  
A: That's expected. The stub doesn't have a real Bluetooth backend. For development/testing, this is fine—your code can still exercise the API.

**Q: How do I know if I'm using the stub or the real SDK?**  
A: Check device properties. The stub returns empty/uninitialized values for real device data (MAC address, name, etc.). A simple check: `device->address(addr); if (addr.empty()) { /* stub */ }`.

## Need Help?

See **INTEGRATION.md** for detailed code examples and patterns.

See **README.md** for full documentation, deployment scenarios, and Yocto recipes.
