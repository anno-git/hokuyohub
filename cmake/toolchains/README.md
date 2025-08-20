# CMake Toolchain Files for Cross-Compilation

This directory contains CMake toolchain files that enable cross-compilation from macOS to various target platforms.

## Available Toolchains

### 1. Linux ARM64 (Raspberry Pi 5) - `linux-arm64.cmake`

Cross-compilation toolchain for building ARM64 Linux binaries targeting Raspberry Pi 5.

**Features:**
- Supports cross-compilation from macOS (Apple Silicon) to linux-arm64
- Optimized for Raspberry Pi 5 (Cortex-A76 CPU)
- Handles ExternalProject cross-compilation (required for urg_library)
- Automatic toolchain detection with multiple fallback strategies
- Sysroot support for better library resolution
- pkg-config configuration for cross-compilation

### 2. Raspberry Pi - `rpi.cmake`

Convenience wrapper that includes the linux-arm64 toolchain. This file exists to maintain compatibility with the CMakePresets.json configuration.

### 3. Windows x64 - `windows-x64.cmake`

Placeholder toolchain for future Windows cross-compilation support. Currently not implemented.

### 4. Common Utilities - `common.cmake`

Shared utilities and functions used by all toolchain files. Provides:
- Tool validation functions
- Cross-compilation environment setup
- ExternalProject configuration helpers
- Sysroot validation
- pkg-config setup for cross-compilation

## Quick Start

### Prerequisites

Install the cross-compilation toolchain:

```bash
# macOS (Homebrew)
brew install aarch64-linux-gnu-gcc

# Ubuntu/Debian
sudo apt-get install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Arch Linux
sudo pacman -S aarch64-linux-gnu-gcc
```

### Basic Usage

Use the CMake preset for Raspberry Pi:

```bash
# Configure
cmake --preset rpi-base

# Build
cmake --build build/linux-arm64

# Install
cmake --install build/linux-arm64
```

### Manual Usage

You can also use the toolchain directly:

```bash
cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake -B build/linux-arm64
cmake --build build/linux-arm64
```

## Advanced Configuration

### Environment Variables

The toolchain supports several environment variables for customization:

- `CROSS_COMPILE_PREFIX`: Override the default `aarch64-linux-gnu-` prefix
- `TARGET_SYSROOT`: Path to target system root directory
- `TOOLCHAIN_ROOT`: Path to toolchain installation directory

Example:
```bash
export CROSS_COMPILE_PREFIX="aarch64-unknown-linux-gnu-"
export TARGET_SYSROOT="/opt/rpi-sysroot"
cmake --preset rpi-base
```

### Sysroot Setup

For better library detection and linking, set up a target sysroot:

1. **Extract from Raspberry Pi OS image:**
   ```bash
   # Mount the Raspberry Pi OS image
   sudo mkdir /mnt/rpi-root
   sudo mount -o loop,offset=272629760 2023-05-03-raspios-bullseye-arm64.img /mnt/rpi-root
   
   # Copy essential directories
   sudo cp -r /mnt/rpi-root/{lib,usr} /opt/rpi-sysroot/
   
   # Set environment variable
   export TARGET_SYSROOT="/opt/rpi-sysroot"
   ```

2. **Use pre-built sysroot:**
   ```bash
   # Download and extract a pre-built sysroot
   wget https://github.com/rvagg/rpi-newer-crosstools/releases/download/v1.0.0/aarch64-rpi4-linux-gnu.tar.xz
   tar -xf aarch64-rpi4-linux-gnu.tar.xz
   export TARGET_SYSROOT="$(pwd)/aarch64-rpi4-linux-gnu/aarch64-rpi4-linux-gnu/sysroot"
   ```

### ExternalProject Integration

The toolchain automatically configures environment variables for ExternalProject builds:

- `CC`: C compiler
- `CXX`: C++ compiler  
- `AR`: Archive tool
- `RANLIB`: Archive index tool
- `AS`: Assembler
- `LD`: Linker
- `CFLAGS`: C compiler flags
- `CXXFLAGS`: C++ compiler flags

This ensures that external projects like urg_library build correctly with cross-compilation.

## Troubleshooting

### Common Issues

1. **Cross-compiler not found:**
   ```
   CMake Error: Could not find cross-compilation gcc
   ```
   **Solution:** Install the cross-compilation toolchain or set `CROSS_COMPILE_PREFIX`.

2. **Libraries not found:**
   ```
   Could NOT find PkgConfig (missing: PKG_CONFIG_EXECUTABLE)
   ```
   **Solution:** Install pkg-config and set up a proper sysroot.

3. **ExternalProject build fails:**
   ```
   make: aarch64-linux-gnu-gcc: No such file or directory
   ```
   **Solution:** Ensure the cross-compiler is in PATH and environment variables are set.

### Debugging

Enable verbose output to see toolchain configuration:

```bash
cmake --preset rpi-base --log-level=STATUS
```

Check toolchain detection:

```bash
cmake --preset rpi-base -DCMAKE_VERBOSE_MAKEFILE=ON
```

### Validation

The toolchain includes built-in validation:

1. **Tool existence:** Verifies that required cross-compilation tools exist
2. **Compiler test:** Attempts to compile a simple test program
3. **Sysroot validation:** Checks for essential directories in the sysroot

## Project Integration

### CMakeLists.txt Considerations

When using cross-compilation, consider these CMake patterns:

```cmake
# Check if cross-compiling
if(CMAKE_CROSSCOMPILING)
    message(STATUS "Cross-compiling for ${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
    
    # Disable tests that can't run on host
    set(BUILD_TESTING OFF)
    
    # Use different install paths
    set(CMAKE_INSTALL_PREFIX "${CMAKE_SOURCE_DIR}/dist-${CMAKE_SYSTEM_NAME}")
endif()

# Find packages with cross-compilation support
find_package(PkgConfig REQUIRED)
pkg_check_modules(EXAMPLE REQUIRED example-lib)
```

### ExternalProject Configuration

For projects using ExternalProject (like urg_library), the toolchain automatically sets up the environment. However, you can also explicitly pass toolchain information:

```cmake
ExternalProject_Add(external_lib
    SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/lib
    CMAKE_ARGS
        -DCMAKE_TOOLCHAIN_FILE=${CMAKE_TOOLCHAIN_FILE}
        -DCMAKE_INSTALL_PREFIX=${CMAKE_BINARY_DIR}/external
    BUILD_COMMAND ${CMAKE_MAKE_PROGRAM} CC=${CMAKE_C_COMPILER} CXX=${CMAKE_CXX_COMPILER}
)
```

## Contributing

When adding new toolchain files:

1. Include the common utilities: `include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)`
2. Set appropriate `CMAKE_SYSTEM_NAME` and `CMAKE_SYSTEM_PROCESSOR`
3. Configure cross-compilation tools with validation
4. Set up ExternalProject environment variables
5. Add comprehensive error messages and usage instructions
6. Update this README with the new toolchain information

## References

- [CMake Cross Compiling](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)
- [CMake ExternalProject](https://cmake.org/cmake/help/latest/module/ExternalProject.html)
- [Raspberry Pi Cross-Compilation](https://www.raspberrypi.org/documentation/linux/kernel/building.md)