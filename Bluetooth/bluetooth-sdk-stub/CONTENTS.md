# Bluetooth SDK Stub Integration Example - Contents

This package contains everything the Bluetooth plugin team needs to integrate the Bluetooth SDK stub into their project.

## What's Included

### Documentation

1. **README.md** — Full feature documentation
   - Architecture overview
   - Build instructions with/without audio support
   - Integration patterns for btmgr
   - Deployment scenarios (stub vs. real SDK)
   - Yocto recipe example

2. **QUICKSTART.md** — Quick reference guide (5-minute start)
   - Build commands
   - CMakeLists.txt snippets
   - Minimal example
   - FAQ

3. **INTEGRATION.md** — Detailed integration examples
   - Step-by-step plugin integration
   - Device discovery and connection examples
   - GATT client operations
   - Audio control examples
   - Event handling patterns
   - Troubleshooting guide

4. **CONTENTS.md** — This file

### Source Code

```
include/
├── Events.h                 # Event emission framework
├── Logger.h                 # Logging support
├── LogRedirect.h            # Log redirection callbacks
├── Status.h                 # Status/error handling
└── bluetooth/
    ├── Manager.h            # Main manager (entry point) ← Start here
    ├── Adapter.h            # Adapter operations (discovery, power)
    ├── Device.h             # Device operations (pairing, connection, properties)
    ├── Audio.h              # Audio control (volume, mute, delay)
    ├── GattClient.h         # GATT client (read/write characteristics)
    ├── GattServer.h         # GATT server (advertise services)
    ├── Advertisement.h      # BLE advertisement management
    ├── Uuid.h               # UUID utilities
    ├── Utils.h              # Utility functions
    ├── Appearance.h         # Bluetooth appearance codes
    ├── GattServices.h       # Standard GATT services
    ├── Connection.h         # Connection management
    └── (all headers are dependency-free)

stub/
├── Manager.cpp              # Stub implementation (no-ops, state tracking)
├── Adapter.cpp
├── Device.cpp
├── Audio.cpp                # Audio stubs (no WirePlumber linking)
├── GattClient.cpp
├── GattServer.cpp
└── Advertisement.cpp
```

### Build Configuration

- **CMakeLists.txt** — Complete build configuration
  - Builds `librdk_bluetooth.so` / `librdk_bluetooth.dylib`
  - No external dependencies (not even BlueZ, sdbus-c++, PipeWire, WirePlumber)
  - `AUDIO_SUPPORT` option for compile-time audio API inclusion
  - Install targets for deployment

## Key Features

✅ **Zero Dependencies** — No BlueZ, D-Bus, sdbus-c++, PipeWire, or WirePlumber required  
✅ **Audio Support** — Include audio method signatures with `-DAUDIO_SUPPORT=ON`  
✅ **ABI Compatible** — Identical symbol set when both stub and real SDK built with same options  
✅ **Minimal Overhead** — ~100KB library, only C++ and system libraries  
✅ **pImpl Design** — All implementation details hidden, API is purely header-based  
✅ **Production Ready** — Can ship stub `.so` for products without SDK  

## Quick Build

```bash
# Default (no audio)
cmake -S . -B build
cmake --build build

# With audio support (stub only; no PipeWire/WirePlumber needed)
cmake -S . -B build -DAUDIO_SUPPORT=ON
cmake --build build
```

**Output:** `build/librdk_bluetooth.so` (Linux) or `build/librdk_bluetooth.dylib` (macOS)

## Integration Paths

### Path 1: Subdirectory in Your Project (Recommended)

```cmake
add_subdirectory(../bluetooth-sdk-stub-integration-example sdk)
target_link_libraries(btmgr PRIVATE rdk_bluetooth)
```

### Path 2: Pre-Built Library

```bash
# Build once
cd bluetooth-sdk-stub-integration-example
cmake -S . -B build -DAUDIO_SUPPORT=ON
cmake --install build --prefix /opt/rdk

# Link in your project
target_include_directories(btmgr PRIVATE /opt/rdk/include)
target_link_libraries(btmgr PRIVATE -L/opt/rdk/lib -lrdk_bluetooth)
```

### Path 3: Yocta Recipe

See **README.md** for a complete Yocta recipe that installs the stub `.so`.

## API Entry Point

Start with **`Manager`** class in `include/bluetooth/Manager.h`:

```cpp
#include <bluetooth/Manager.h>

bluetooth::Manager manager;
std::shared_ptr<bluetooth::Adapter> adapter;
manager.getDefaultAdapter(adapter);
```

Then explore:
- `Adapter` — Device discovery and enumeration
- `Device` — Pairing, connection, properties
- `GattClient` / `GattServer` — GATT operations
- `Audio` — Volume, mute, delay compensation (when `AUDIO_SUPPORT=ON`)

## File Manifest

| File | Purpose | Size |
|------|---------|------|
| CMakeLists.txt | Build configuration | ~1.5 KB |
| README.md | Full documentation | ~6 KB |
| QUICKSTART.md | Quick reference | ~4 KB |
| INTEGRATION.md | Integration examples | ~10 KB |
| CONTENTS.md | This file | ~2 KB |
| include/*.h | Support headers | ~2 KB |
| include/bluetooth/*.h | SDK public API | ~40 KB |
| stub/*.cpp | Stub implementations | ~30 KB |
| **Total** | | **~95 KB** |

## Next Steps

1. **Read QUICKSTART.md** (5 minutes) — Get the package building
2. **Read INTEGRATION.md** (15 minutes) — See examples of how to use the API
3. **Add to your CMakeLists.txt** — Follow the integration patterns
4. **Start with Manager::getDefaultAdapter()** — Get an adapter and explore from there
5. **Refer to README.md** — For deployment and production patterns

## Support

- **Build issues:** Check CMAKE_CXX_COMPILER_ID and CMAKE_BUILD_TYPE
- **Link errors:** Ensure `target_link_libraries(... rdk_bluetooth Threads::Threads)`
- **API questions:** See INTEGRATION.md examples
- **Deployment:** See README.md deployment section

## License

These files are part of the RDK Bluetooth SDK and are licensed under Apache License 2.0.

---

**Version:** 1.0 (Bluetooth SDK stub, pImpl-based audio isolation)  
**Last Updated:** September 3, 2026  
**Parent Repository:** https://github.com/rdkcentral/bluetooth-sdk  
