# Bluetooth SDK Stub Integration Example

This folder contains a minimal, standalone example of the Bluetooth SDK stub implementation for integration into the Bluetooth plugin (btmgr).

## Structure

```
.
├── CMakeLists.txt           # Build configuration
├── include/
│   ├── *.h                  # Support headers (Events, Status, Logger, LogRedirect)
│   └── bluetooth/
│       └── *.h              # Public Bluetooth SDK API headers
└── stub/
    └── *.cpp                # Fake implementation with no BlueZ/D-Bus/sdbus-c++ dependency
```

## Key Features

- **Zero External Dependencies:** No BlueZ, D-Bus, sdbus-c++, PipeWire, or WirePlumber required.
- **Compile-Time Audio API:** Audio method signatures are available when built with `-DAUDIO_SUPPORT=ON`, but the resulting library has no WirePlumber runtime dependency.
- **SDK ABI Compatible:** Exports the same public symbols as the real SDK when built with matching options.
- **pImpl-Based Design:** All D-Bus and implementation details are hidden behind public class pointers, making the API dependency-free.

## Build

### Default (no audio support)

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
cmake --build . --parallel
```

### With Audio API Support

```bash
mkdir build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DAUDIO_SUPPORT=ON ..
cmake --build . --parallel
```

Result: `build/librdk_bluetooth.{so,dylib}` with no WirePlumber or PipeWire runtime dependencies.
and
## Library Versioning

The library is built with semantic versioning:

```cmake
set_target_properties(rdk_bluetooth PROPERTIES
    VERSION 1.0.0      # Full version
    SOVERSION 1        # ABI version for compatibility
)
```

This creates versioned symlinks on installation:

```bash
librdk_bluetooth.so.1.0.0    # Actual library with full version
librdk_bluetooth.so.1        # Symlink for ABI compatibility
librdk_bluetooth.so          # Generic symlink (for linking)
```

### Why Versioning Matters

- **Version Tracking:** Easily identify which SDK version is deployed (`1.0.0`).
- **ABI Compatibility:** Applications compiled against `librdk_bluetooth.so.1` continue to work even if patch versions change (`1.0.1`, `1.0.2`, etc.).
- **Multiple Versions:** Support multiple major versions in the same system if needed (e.g., `librdk_bluetooth.so.1` and `librdk_bluetooth.so.2`).

### Check Library Version

After building:

```bash
ls -la build/librdk_bluetooth.so*
# Output:
# -rw-r--r-- librdk_bluetooth.so.1.0.0
# lrwxrwxrwx librdk_bluetooth.so.1 -> librdk_bluetooth.so.1.0.0
# lrwxrwxrwx librdk_bluetooth.so -> librdk_bluetooth.so.1

# View soname with ldd
ldd -r build/librdk_bluetooth.so
# Outputs: librdk_bluetooth.so.1 as soname
```

### Update Version

To release a new version, edit [CMakeLists.txt](CMakeLists.txt):

```cmake
set_target_properties(rdk_bluetooth PROPERTIES
    VERSION 1.1.0      # Bump minor version
    SOVERSION 1        # Keep ABI version if compatible
)
```

For breaking ABI changes, increment `SOVERSION`:

```cmake
set_target_properties(rdk_bluetooth PROPERTIES
    VERSION 2.0.0      # Major version bump
    SOVERSION 2        # New ABI version
)
```

## Linking the Library: Build and Runtime

### Build-Time Linking

Your Bluetooth plugin links against the versioned library at build time. CMake automatically selects the correct symlink:

**In your plugin's CMakeLists.txt:**
```cmake
# Link against the SDK (CMake handles version symlink resolution)
target_link_libraries(btplugin_binary PRIVATE rdk_bluetooth Threads::Threads)

# Or explicitly find pre-built library
find_library(RDK_BLUETOOTH rdk_bluetooth REQUIRED)
target_link_libraries(btplugin_binary PRIVATE ${RDK_BLUETOOTH})
```

CMake will link against `librdk_bluetooth.so` (or `.so.1` if only that exists), and the binary stores the SONAME (`librdk_bluetooth.so.1`) in its ELF header.

**Verify linking:**
```bash
ldd ./btplugin_binary | grep rdk_bluetooth
# Output: librdk_bluetooth.so.1 => /usr/lib/librdk_bluetooth.so.1 (0x...)
```

### Runtime Library Resolution

At runtime, the linker searches for the library using this order:

1. Paths in `LD_LIBRARY_PATH` (if set)
2. Paths in `rpath` (embedded in binary, if set during build)
3. System library paths (`/lib`, `/usr/lib`, etc.)

#### Approach 1: Using LD_LIBRARY_PATH (Recommended for Yocta)

This is used in the service script below. The runtime linker finds the correct version automatically:

```bash
export LD_LIBRARY_PATH=/usr/lib/btsdk:/usr/lib/vendor
exec /path/to/btplugin.bin
# Linker resolves: SONAME "librdk_bluetooth.so.1" -> finds /usr/lib/btsdk/librdk_bluetooth.so.1 -> actual file /usr/lib/btsdk/librdk_bluetooth.so.1.0.0
```

#### Approach 2: Using RPATH (For Embedded/Custom Paths)

If you want the binary to find the library without environment variables, set `RPATH` at build time:

```cmake
set_target_properties(btplugin_binary PROPERTIES
    BUILD_RPATH "/usr/local/lib"
    INSTALL_RPATH "/usr/lib/btsdk;/usr/lib/vendor"
)
```

Then the binary finds the library automatically regardless of `LD_LIBRARY_PATH`.

## Integration into btmgr

### 1. Include the SDK Headers

Copy the `include/bluetooth/` directory to your middleware include paths:

```bash
cp -r include/bluetooth /path/to/middleware/include/
```

### 2. Link Against the Stub Library

In your middleware CMakeLists.txt:

```cmake
add_executable(btmgr ...)

target_include_directories(btmgr PRIVATE /path/to/sdk/include)
target_link_libraries(btmgr PRIVATE rdk_bluetooth Threads::Threads)
```

### 3. Optional: Build the Stub Separately

If you want a standalone `.so` to ship with products without the real SDK:

```bash
cd /path/to/bluetooth-sdk-stub
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
cmake --install build --prefix=/usr/local
```

Then link your middleware:

```cmake
find_library(RDK_BLUETOOTH rdk_bluetooth REQUIRED)
target_link_libraries(btmgr PRIVATE ${RDK_BLUETOOTH})
```

## Public API

The stub implements these key classes:

- **Manager:** Adapter lifecycle and WirePlumber integration (stub: no-op).
- **Adapter:** Discovery, power, and device enumeration (stub: basic state management).
- **Device:** Pairing, connection, properties, and GATT (stub: no-op with state tracking).
- **GattClient / GattServer:** GATT operations (stub: minimal).
- **Advertisement / AdvertisementMgr:** BLE advertisement (stub: no-op).
- **Audio:** Volume, mute, and delay compensation (stub: no-op when `AUDIO_SUPPORT=ON`).

All methods return `Status::BLUETOOTH_ERROR` with "stub: no Bluetooth backend available" on real device operations. State changes (e.g., `Device::state()`) are tracked locally for integration testing.

## Audio Support

When compiled with `-DAUDIO_SUPPORT=ON`:

- Public headers export audio method signatures.
- Constructor signatures match the real SDK (e.g., `Device::initAudio(WpNode*, WpProxy*)`).
- Stub implementations are no-ops; they do not call or link WirePlumber.
- **No WirePlumber headers or libraries are required at compile or runtime.**

This allows middleware to compile against audio-enabled SDK headers and link the stub without installing WirePlumber development packages.

## Deployment

### For Products Without the Real SDK

1. Build the stub with your target audio configuration.
2. Install `librdk_bluetooth.so` at runtime.
3. Middleware linked against the stub will have matching audio symbols available.

### For Products With the Real SDK

1. Build the real SDK (requires sdbus-c++, BlueZ, PipeWire/WirePlumber).
2. At runtime, replace the stub `.so` with the real implementation at the same path.
3. Middleware continues to work with no code changes.

## CMake Variables

| Variable | Default | Effect |
|----------|---------|--------|
| `AUDIO_SUPPORT` | `OFF` | Enable audio method compilation. No external dependencies required. |
| `CMAKE_BUILD_TYPE` | (unset) | Set to `Release` or `Debug` for optimization. |

## Example Yocto Recipe

```bitbake
SUMMARY = "Bluetooth SDK stub library"
LICENSE = "Apache-2.0"

SRC_URI = "git://your-repo/bluetooth-sdk-stub;protocol=ssh"
SRCREV = "${AUTOREV}"

S = "${WORKDIR}/git"

inherit cmake

EXTRA_OECMAKE = "-DAUDIO_SUPPORT=ON"

do_install:append() {
    install -D -m 0755 ${B}/librdk_bluetooth.so ${D}${libdir}/librdk_bluetooth.so
    install -d ${D}${includedir}/bluetooth
    install -m 0644 ${S}/include/bluetooth/*.h ${D}${includedir}/bluetooth/
}
```

## Runtime Deployment: Selecting Stub vs. Real SDK

At runtime, you can conditionally use either the real SDK (if available) or the stub library without rebuilding. This pattern allows a single Yocta image to adapt at boot time:

### Systemd Service Script

In your Bluetooth plugin systemd service or init script, set `LD_LIBRARY_PATH` based on what's available. The linker automatically resolves the SONAME to the versioned file:

**Simple Version (no compatibility check):**
```bash
#!/bin/bash

# Select middleware (real SDK) or vendor (stub) implementation at runtime
# The linker resolves SONAME "librdk_bluetooth.so.1" to the actual versioned file
if [ -f /usr/lib/btsdk/librdk_bluetooth.so.1 ]; then
    # Real SDK available: use middleware library path
    export LD_LIBRARY_PATH=/usr/lib/btsdk:/usr/lib/mw
else
    # No real SDK: use vendor stub library
    export LD_LIBRARY_PATH=/usr/lib/vendor
fi

# Start the Bluetooth plugin
exec /path/to/bluetoothplugin.bin
```

**Advanced Version (with version compatibility check):**

This version verifies that both real and stub implementations are compatible (matching ABI version):

```bash
#!/bin/bash
set -e

PLUGIN_BIN="/path/to/bluetoothplugin.bin"
REAL_SDK="/usr/lib/btsdk/librdk_bluetooth.so.1"
VENDOR_SDK="/usr/lib/vendor/librdk_bluetooth.so.1"

# Function to get library ABI version (SOVERSION)
get_soversion() {
    local lib=$1
    if [ -f "$lib" ]; then
        # Extract SOVERSION from readelf (ELF format on Linux)
        readelf -d "$lib" 2>/dev/null | grep SONAME | grep -oE 'librdk_bluetooth\.so\.[0-9]+' | grep -oE '[0-9]+$' || echo "unknown"
    else
        echo "missing"
    fi
}

# Check if real SDK is available
if [ -f "$REAL_SDK" ]; then
    REAL_VERSION=$(get_soversion "$REAL_SDK")
    
    # If stub is also available, verify they're compatible
    if [ -f "$VENDOR_SDK" ]; then
        VENDOR_VERSION=$(get_soversion "$VENDOR_SDK")
        
        if [ "$REAL_VERSION" != "$VENDOR_VERSION" ]; then
            echo "WARNING: Real SDK ABI ($REAL_VERSION) and Vendor SDK ABI ($VENDOR_VERSION) mismatch!"
            echo "  Real SDK:  $REAL_SDK"
            echo "  Vendor SDK: $VENDOR_SDK"
            echo "  Proceeding with Real SDK (preferred when available)"
        fi
    fi
    
    # Use real SDK
    export LD_LIBRARY_PATH=/usr/lib/btsdk:/usr/lib/mw:${LD_LIBRARY_PATH}
    echo "INFO: Using Real SDK from /usr/lib/btsdk (ABI version $REAL_VERSION)"
    
elif [ -f "$VENDOR_SDK" ]; then
    # Fallback to vendor stub
    VENDOR_VERSION=$(get_soversion "$VENDOR_SDK")
    export LD_LIBRARY_PATH=/usr/lib/vendor:${LD_LIBRARY_PATH}
    echo "INFO: Using Vendor Stub from /usr/lib/vendor (ABI version $VENDOR_VERSION)"
else
    echo "ERROR: Neither Real SDK ($REAL_SDK) nor Vendor SDK ($VENDOR_SDK) found"
    exit 1
fi

# Verify the binary can find the library
if ! ldd -r "$PLUGIN_BIN" 2>/dev/null | grep -q librdk_bluetooth.so.1; then
    echo "ERROR: librdk_bluetooth.so.1 not found for $PLUGIN_BIN"
    echo "LD_LIBRARY_PATH=$LD_LIBRARY_PATH"
    exit 1
fi

# Start the Bluetooth plugin
exec "$PLUGIN_BIN"
```

**Version Matching Strategy:**

- **SOVERSION Match:** Both real and stub should have same ABI version (e.g., `.so.1`)
- **VERSION May Differ:** Full versions can differ (`1.0.0` vs `1.0.1`) if SOVERSION matches
- **Prefer Real SDK:** If both available and compatible, real SDK is used
- **Warn on Mismatch:** If versions don't match, a warning is logged but real SDK is still preferred
- **Fallback:** If only stub available, it's used regardless of version

### File Layout

After installation, your library paths contain versioned files and symlinks:

**Real SDK Installation:**
```
/usr/lib/btsdk/
├── librdk_bluetooth.so.1.0.0        # Actual library file
├── librdk_bluetooth.so.1 -> librdk_bluetooth.so.1.0.0    # ABI symlink
└── librdk_bluetooth.so -> librdk_bluetooth.so.1           # Convenience symlink
```

**Stub SDK Installation:**
```
/usr/lib/vendor/
├── librdk_bluetooth.so.1.0.0        # Actual library file
├── librdk_bluetooth.so.1 -> librdk_bluetooth.so.1.0.0    # ABI symlink
└── librdk_bluetooth.so -> librdk_bluetooth.so.1           # Convenience symlink
```

### Corresponding Yocta Recipe Paths

Ensure your Yocta recipes install to the expected paths. The versioning creates symlinks automatically during install:

**Real SDK Recipe:**
```bitbake
do_install:append() {
    # CMake install creates:
    #   ${libdir}/btsdk/librdk_bluetooth.so.1.0.0 (actual)
    #   ${libdir}/btsdk/librdk_bluetooth.so.1 -> librdk_bluetooth.so.1.0.0 (symlink)
    #   ${libdir}/btsdk/librdk_bluetooth.so -> librdk_bluetooth.so.1 (symlink)
    install -D -m 0755 ${B}/librdk_bluetooth.so* ${D}${libdir}/btsdk/
}
```

**Stub SDK Recipe:**
```bitbake
do_install:append() {
    # CMake install creates:
    #   ${libdir}/vendor/librdk_bluetooth.so.1.0.0 (actual)
    #   ${libdir}/vendor/librdk_bluetooth.so.1 -> librdk_bluetooth.so.1.0.0 (symlink)
    #   ${libdir}/vendor/librdk_bluetooth.so -> librdk_bluetooth.so.1 (symlink)
    install -D -m 0755 ${B}/librdk_bluetooth.so* ${D}${libdir}/vendor/
}
```

The wildcard `librdk_bluetooth.so*` captures all versioned files and symlinks.

### Benefits

- **Single Image:** Both stub and real SDK can coexist in the same image.
- **Runtime Selection:** Service script picks the correct library at boot time.
- **No Rebuild:** Product configuration changes don't require recompilation.
- **Fallback:** Automatically falls back to stub if real SDK is unavailable.
- **Versioning Stability:** ABI version (`librdk_bluetooth.so.1`) stays the same while patch versions change (`1.0.0` → `1.0.1`), so applications continue working.
- **Library Upgrade Path:** Install `1.0.1` or `1.1.0` without relinking binaries that use SONAME `.so.1`.

### Complete Example: Bluetooth Plugin Integration

Your **btplugin/CMakeLists.txt** links against the versioned library:

```cmake
cmake_minimum_required(VERSION 3.16)
project(btplugin)

find_package(Threads REQUIRED)

# Link against the SDK (versioning handled by linker)
add_executable(btplugin src/main.cpp src/plugin.cpp)
target_link_libraries(btplugin PRIVATE rdk_bluetooth Threads::Threads)
target_include_directories(btplugin PRIVATE /path/to/sdk/include)

# Optional: embed rpath so library is found without LD_LIBRARY_PATH
set_target_properties(btplugin PROPERTIES
    INSTALL_RPATH "/usr/lib/btsdk:/usr/lib/vendor"
)
```

Your **btplugin-service.sh** selects the library at runtime with version checking:

```bash
#!/bin/bash
set -e

REAL_SDK="/usr/lib/btsdk/librdk_bluetooth.so.1"
VENDOR_SDK="/usr/lib/vendor/librdk_bluetooth.so.1"

# Helper: Get ABI version from library
get_abi_version() {
    local lib=$1
    if [ -f "$lib" ]; then
        # Extract soname version from readelf output
        readelf -d "$lib" 2>/dev/null | grep SONAME | grep -oE '\.so\.[0-9]+' | sed 's/.so.//' || echo "unknown"
    else
        echo "missing"
    fi
}

# Select SDK at runtime with version checking
if [ -f "$REAL_SDK" ]; then
    REAL_ABI=$(get_abi_version "$REAL_SDK")
    
    # Check vendor for compatibility
    if [ -f "$VENDOR_SDK" ]; then
        VENDOR_ABI=$(get_abi_version "$VENDOR_SDK")
        if [ "$REAL_ABI" != "$VENDOR_ABI" ]; then
            echo "⚠️  Version mismatch: Real SDK ABI $REAL_ABI vs Vendor ABI $VENDOR_ABI (using Real SDK)"
        fi
    fi
    
    export LD_LIBRARY_PATH=/usr/lib/btsdk:${LD_LIBRARY_PATH}
    echo "✓ Using Real SDK (ABI version $REAL_ABI)"
else
    VENDOR_ABI=$(get_abi_version "$VENDOR_SDK")
    export LD_LIBRARY_PATH=/usr/lib/vendor:${LD_LIBRARY_PATH}
    echo "✓ Using Vendor Stub (ABI version $VENDOR_ABI)"
fi

# Start plugin
exec /usr/bin/btplugin
```

**At runtime, the linker resolves:**
```
Binary requests SONAME: librdk_bluetooth.so.1
Linker searches LD_LIBRARY_PATH: /usr/lib/btsdk or /usr/lib/vendor
Finds: librdk_bluetooth.so.1 (symlink)
Follows symlink: librdk_bluetooth.so.1.0.0 (actual file)
Loads library into memory
```

### Example: Yocta Integration

Create a Bluetooth plugin recipe that works with both implementations:

```bitbake
SUMMARY = "Bluetooth Plugin"
LICENSE = "Apache-2.0"

DEPENDS = "librdk-bluetooth"
RDEPENDS:${PN} = "librdk-bluetooth"

# Install the startup script
do_install() {
    install -D -m 0755 ${S}/btplugin-service.sh ${D}${bindir}/btplugin-service.sh
}

# Systemd service
inherit systemd
SYSTEMD_SERVICE:${PN} = "btplugin.service"

do_install:append() {
    install -D -m 0644 ${S}/btplugin.service \
        ${D}${systemd_unitdir}/system/btplugin.service
}
```

**btplugin.service:**
```ini
[Unit]
Description=Bluetooth Plugin Service
After=network.target

[Service]
Type=simple
ExecStart=/usr/bin/btplugin-service.sh
Restart=always
RestartSec=5

[Install]
WantedBy=multi-user.target
```

## Notes

- The stub is **not** a real Bluetooth implementation; it provides stubs and no-ops for all hardware operations.
- For testing/CI, middleware can link the stub and exercise its API without actual Bluetooth hardware.
- The stub's symbol set matches the real SDK when both are built with identical `-DAUDIO_SUPPORT=` settings.
- To see integration patterns, refer to the parent `bluetooth-sdk` repository at `src/bluetooth/` (real implementation) and `stub/` (this example).
