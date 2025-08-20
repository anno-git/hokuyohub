# Cross-Compilation Fixes for Raspberry Pi

## Overview

This document describes the fixes implemented to resolve cross-compilation issues for Raspberry Pi 5 (ARM64) builds on macOS.

## Issues Fixed

### 1. Toolchain Detection Failure
**Problem**: The toolchain file only looked for `aarch64-linux-gnu-*` tools but the available toolchain was `aarch64-linux-musl-*`.

**Solution**: Updated [`cmake/toolchains/linux-arm64.cmake`](../cmake/toolchains/linux-arm64.cmake) to support both GNU and musl toolchains:
- Added `aarch64-linux-musl-` to the `TOOL_PREFIXES` list
- Updated all tool search functions to include musl variants
- Added musl sysroot detection for `/opt/homebrew/Cellar/musl-cross/*/libexec/aarch64-linux-musl`

### 2. Environment Variable Recognition
**Problem**: `CROSS_COMPILE_PREFIX` environment variable was not properly recognized during toolchain loading.

**Solution**: The toolchain file already had proper environment variable handling. Updated [`CMakePresets.json`](../CMakePresets.json) to use the correct prefix:
```json
"environment": {
  "CROSS_COMPILE_PREFIX": "aarch64-linux-musl-",
  "PKG_CONFIG_PATH": "",
  "PKG_CONFIG_LIBDIR": ""
}
```

### 3. Linker Flag Compatibility
**Problem**: GNU linker flags (`--as-needed`) were incompatible with macOS linker when used incorrectly.

**Solution**: Added conditional linker flag handling in [`cmake/toolchains/linux-arm64.cmake`](../cmake/toolchains/linux-arm64.cmake):
```cmake
# Linker flags - conditional based on platform and toolchain
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # For cross-compilation to Linux, use GNU linker flags
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--as-needed")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--as-needed")
else()
    # For other platforms, don't use GNU-specific flags
    set(CMAKE_EXE_LINKER_FLAGS_INIT "")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "")
endif()
```

### 4. Setup Script Compatibility
**Problem**: Setup script only supported GNU toolchain installation.

**Solution**: Updated [`scripts/setup_cross_compile.sh`](../scripts/setup_cross_compile.sh) to support both toolchains:
- Added separate tool arrays for GNU and musl toolchains
- Updated installation logic to try musl-cross first, then fallback to GNU
- Enhanced tool checking to detect both toolchain types
- Updated environment variable documentation

## Verification

### Toolchain Detection Test
```bash
cmake --preset rpi-release
```

**Expected Output**:
```
-- Using cross-compile prefix: aarch64-linux-musl-
-- Found cross-compilation gcc: /opt/homebrew/Cellar/musl-cross/0.9.9_2/bin/aarch64-linux-musl-gcc
-- Found cross-compilation g++: /opt/homebrew/Cellar/musl-cross/0.9.9_2/bin/aarch64-linux-musl-g++
-- Found cross-compilation ar: /opt/homebrew/Cellar/musl-cross/0.9.9_2/bin/aarch64-linux-musl-ar
-- Found cross-compilation ranlib: /opt/homebrew/Cellar/musl-cross/0.9.9_2/bin/aarch64-linux-musl-ranlib
```

### Binary Validation Test
```bash
# Compile test program
aarch64-linux-musl-g++ -o test_cross_compile_arm64 test_cross_compile.cpp

# Verify architecture
file test_cross_compile_arm64
```

**Expected Output**:
```
test_cross_compile_arm64: ELF 64-bit LSB executable, ARM aarch64, version 1 (SYSV), dynamically linked, interpreter /lib/ld-musl-aarch64.so.1, not stripped
```

## Installation Instructions

### Install musl-cross Toolchain
```bash
# Install via Homebrew (recommended)
brew install musl-cross

# Or use the setup script
./scripts/setup_cross_compile.sh
```

### Configure Environment (Optional)
```bash
# For musl toolchain (default)
export CROSS_COMPILE_PREFIX="aarch64-linux-musl-"

# For GNU toolchain (if available)
export CROSS_COMPILE_PREFIX="aarch64-linux-gnu-"

# Optional: Set custom sysroot
export TARGET_SYSROOT="/path/to/raspberry-pi-sysroot"
```

### Build Commands
```bash
# Configure for Raspberry Pi 5 Release
cmake --preset rpi-release

# Build the project
cmake --build build/linux-arm64

# Install to distribution directory
cmake --build build/linux-arm64 --target install
```

## Supported Toolchains

The cross-compilation system now supports:

1. **musl-cross** (Primary)
   - Package: `musl-cross` (Homebrew)
   - Prefix: `aarch64-linux-musl-`
   - Sysroot: `/opt/homebrew/Cellar/musl-cross/*/libexec/aarch64-linux-musl`

2. **GNU Cross-Compiler** (Fallback)
   - Package: `aarch64-linux-gnu-gcc` (Homebrew)
   - Prefix: `aarch64-linux-gnu-`
   - Sysroot: `/opt/homebrew/Cellar/aarch64-linux-gnu-gcc/*/toolchain/aarch64-linux-gnu/sysroot`

## Compatibility

- ✅ **macOS**: Fully supported (Apple Silicon and Intel)
- ✅ **Linux**: Should work with appropriate toolchain packages
- ✅ **Windows**: Should work with WSL2 and appropriate toolchain

## Known Limitations

1. **Dependency Libraries**: Some dependencies (like Jsoncpp, OpenSSL) may need to be cross-compiled or provided in the target sysroot for full functionality.

2. **Sysroot**: While the toolchain works without a sysroot, having a proper Raspberry Pi sysroot improves library detection and linking.

3. **Testing**: Cross-compiled binaries cannot be executed on the host system and require target hardware or emulation for runtime testing.

## Troubleshooting

### Toolchain Not Found
```bash
# Check if toolchain is installed
which aarch64-linux-musl-gcc

# Run setup script to install
./scripts/setup_cross_compile.sh
```

### CMake Configuration Fails
```bash
# Clean build directory
rm -rf build/linux-arm64

# Reconfigure with verbose output
cmake --preset rpi-release --verbose
```

### Wrong Architecture Binary
```bash
# Verify binary architecture
file your_binary

# Should show: ARM aarch64