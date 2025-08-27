# Phase 2: Cross-Compilation Build System

This document describes the Phase 2 implementation of the cross-compilation build system for targeting Raspberry Pi 5 (linux-arm64) from macOS development environments.

## Overview

Phase 2 introduces cross-compilation capabilities while maintaining complete isolation from the existing macOS build system established in Phase 1. The cross-compilation system is entirely opt-in and uses dedicated build directories.

## Architecture

### Directory Structure

```
proj/
├── build/
│   ├── darwin-arm64/          # macOS builds (Phase 1)
│   └── linux-arm64/           # Cross-compilation builds (Phase 2)
├── dist/
│   ├── darwin-arm64/          # macOS distribution
│   └── linux-arm64/           # Cross-compilation distribution
├── cmake/toolchains/
│   ├── common.cmake           # Shared toolchain utilities
│   └── linux-arm64.cmake     # Raspberry Pi 5 toolchain
└── scripts/
    ├── setup_cross_compile.sh # Environment setup script
    └── cross_build.sh         # Cross-compilation build script
```

### CMake Presets

Phase 2 adds the following cross-compilation presets:

- **`rpi-base`**: Base configuration for Raspberry Pi 5
- **`rpi-debug`**: Debug build with development flags
- **`rpi-release`**: Optimized release build
- **`rpi-relwithdebinfo`**: Release with debug information

## Prerequisites

### Required Tools

Cross-compilation requires the following tools:

- `aarch64-linux-gnu-gcc` - C compiler
- `aarch64-linux-gnu-g++` - C++ compiler  
- `aarch64-linux-gnu-ar` - Archive tool
- `aarch64-linux-gnu-ranlib` - Archive indexer

### Installation

#### macOS (Homebrew)

```bash
# Install cross-compilation toolchain
brew install aarch64-linux-gnu-gcc

# Verify installation
aarch64-linux-gnu-gcc --version
```

#### Automated Setup

Use the provided setup script:

```bash
# Check current environment
./scripts/setup_cross_compile.sh --check-only

# Set up environment (interactive)
./scripts/setup_cross_compile.sh
```

## Environment Configuration

### Environment Variables

The following environment variables can be used to customize cross-compilation:

| Variable | Description | Default |
|----------|-------------|---------|
| `CROSS_COMPILE_PREFIX` | Cross-compiler prefix | `aarch64-linux-gnu-` |
| `TARGET_SYSROOT` | Path to target sysroot | Auto-detected |
| `TOOLCHAIN_ROOT` | Toolchain installation path | `/opt/homebrew` |

### Setting Environment Variables

Add to your shell profile (`~/.zshrc` or `~/.bashrc`):

```bash
# Optional: Override cross-compile prefix
export CROSS_COMPILE_PREFIX="aarch64-linux-gnu-"

# Optional: Set target sysroot for better library detection
export TARGET_SYSROOT="/path/to/raspberry-pi-sysroot"

# Optional: Set toolchain root directory
export TOOLCHAIN_ROOT="/opt/homebrew"
```

### Sysroot Configuration

A sysroot improves library detection and linking. While optional, it's recommended for production builds.

#### Obtaining a Sysroot

1. **From Raspberry Pi OS Image**: Extract from official Raspberry Pi OS
2. **From Running Pi**: Copy system directories via `rsync`
3. **Homebrew**: Automatically detected if available

#### Sysroot Structure

A proper sysroot should contain:

```
sysroot/
├── usr/
│   ├── include/           # System headers
│   ├── lib/              # System libraries
│   └── lib/aarch64-linux-gnu/  # Architecture-specific libraries
└── lib/                  # Essential libraries
```

## Usage

### Quick Start

1. **Set up environment**:
   ```bash
   ./scripts/setup_cross_compile.sh
   ```

2. **Build for Raspberry Pi 5**:
   ```bash
   ./scripts/cross_build.sh --preset rpi-release
   ```

3. **Install distribution**:
   ```bash
   ./scripts/cross_build.sh --preset rpi-release --install
   ```

### Manual CMake Usage

#### Configure

```bash
# Configure for release build
cmake --preset rpi-release

# Configure for debug build
cmake --preset rpi-debug
```

#### Build

```bash
# Build all targets
cmake --build build/linux-arm64

# Build specific target
cmake --build build/linux-arm64 --target hokuyo_hub

# Parallel build
cmake --build build/linux-arm64 --parallel 4
```

#### Install

```bash
# Install to dist/linux-arm64/
cmake --build build/linux-arm64 --target install
```

### Advanced Usage

#### Custom Environment

```bash
# Use custom sysroot
export TARGET_SYSROOT="/path/to/custom/sysroot"
cmake --preset rpi-release

# Use different compiler prefix
export CROSS_COMPILE_PREFIX="aarch64-unknown-linux-gnu-"
cmake --preset rpi-release
```

#### Clean Builds

```bash
# Clean and rebuild
./scripts/cross_build.sh --preset rpi-release --clean

# Manual clean
rm -rf build/linux-arm64
cmake --preset rpi-release
```

## Build Presets Reference

### Configure Presets

| Preset | Description | Build Type | Optimization |
|--------|-------------|------------|--------------|
| `rpi-base` | Base configuration | None | None |
| `rpi-debug` | Development build | Debug | `-g -O0` |
| `rpi-release` | Production build | Release | `-O3` |
| `rpi-relwithdebinfo` | Release with debug info | RelWithDebInfo | `-O2 -g` |

### Build Presets

| Preset | Description | Configure Preset |
|--------|-------------|------------------|
| `build-rpi-debug` | Build debug version | `rpi-debug` |
| `build-rpi-release` | Build release version | `rpi-release` |
| `build-rpi-relwithdebinfo` | Build with debug info | `rpi-relwithdebinfo` |
| `build-all-rpi` | Build all targets | `rpi-release` |

### Package Presets

| Preset | Description | Configure Preset |
|--------|-------------|------------------|
| `package-rpi-release` | Create distribution package | `rpi-release` |

## Compiler Flags

### Target-Specific Optimizations

The toolchain includes Raspberry Pi 5 specific optimizations:

```cmake
# CPU-specific flags for Cortex-A76 (Raspberry Pi 5)
set(RPI5_CPU_FLAGS "-mcpu=cortex-a76 -mtune=cortex-a76")

# Common flags
set(COMMON_FLAGS "${RPI5_CPU_FLAGS} -fPIC -fstack-protector-strong")
```

### Build Type Flags

| Build Type | C/C++ Flags | Description |
|------------|-------------|-------------|
| Debug | `-g -O0` | No optimization, debug symbols |
| Release | `-O3 -DNDEBUG` | Full optimization, no debug |
| RelWithDebInfo | `-O2 -g -DNDEBUG` | Optimized with debug symbols |
| MinSizeRel | `-Os -DNDEBUG` | Size optimization |

## Troubleshooting

### Common Issues

#### 1. Cross-Compiler Not Found

**Error**: `Could not find cross-compilation gcc`

**Solution**:
```bash
# Install toolchain
brew install aarch64-linux-gnu-gcc

# Verify installation
which aarch64-linux-gnu-gcc
```

#### 2. Library Not Found

**Error**: `cannot find -lsomelib`

**Solutions**:
- Set up proper sysroot with target libraries
- Install development packages in sysroot
- Check `PKG_CONFIG_LIBDIR` configuration

#### 3. Header Files Missing

**Error**: `fatal error: 'header.h' file not found`

**Solutions**:
- Ensure sysroot contains target headers
- Verify sysroot structure
- Check include paths in toolchain

#### 4. Linking Errors

**Error**: `undefined reference to symbol`

**Solutions**:
- Verify all required libraries are available in sysroot
- Check library search paths
- Ensure compatible library versions

### Debugging Tips

#### 1. Verbose Output

```bash
# Enable verbose CMake output
cmake --preset rpi-release --verbose

# Enable verbose build output
cmake --build build/linux-arm64 --verbose
```

#### 2. Environment Inspection

```bash
# Check toolchain detection
./scripts/setup_cross_compile.sh --check-only

# Inspect CMake configuration
cmake --preset rpi-release --debug-output
```

#### 3. Manual Tool Testing

```bash
# Test compiler directly
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-gcc -v -E - < /dev/null

# Test compilation
echo 'int main(){return 0;}' | aarch64-linux-gnu-gcc -x c - -o test
file test
```

## Integration with CI/CD

### GitHub Actions Example

```yaml
name: Cross-Compile for Raspberry Pi 5

on: [push, pull_request]

jobs:
  cross-compile:
    runs-on: ubuntu-latest
    steps:
      - uses: actions/checkout@v3
      
      - name: Install cross-compilation tools
        run: |
          sudo apt-get update
          sudo apt-get install -y gcc-aarch64-linux-gnu g++-aarch64-linux-gnu
      
      - name: Configure for Raspberry Pi 5
        run: cmake --preset rpi-release
      
      - name: Build
        run: cmake --build build/linux-arm64
      
      - name: Package
        run: cmake --build build/linux-arm64 --target install
```

## Isolation Verification

Phase 2 maintains complete isolation from Phase 1 macOS builds:

### Separate Build Directories

- macOS builds: `build/darwin-arm64/`
- Cross builds: `build/linux-arm64/`

### Separate Install Directories

- macOS installs: `dist/darwin-arm64/`
- Cross installs: `dist/linux-arm64/`

### Independent Presets

- macOS presets: `mac-*`
- Cross presets: `rpi-*`

### Verification Commands

```bash
# Build both systems independently
cmake --preset mac-release
cmake --build build/darwin-arm64

cmake --preset rpi-release  
cmake --build build/linux-arm64

# Verify isolation
ls -la build/        # Should show both directories
ls -la dist/         # Should show both distributions
```

## Performance Considerations

### Build Performance

- Use parallel builds: `--parallel N`
- Enable ccache if available
- Use faster storage for build directories

### Runtime Performance

- Release builds include Raspberry Pi 5 optimizations
- Consider profile-guided optimization for critical paths
- Monitor memory usage on target hardware

## Security Considerations

### Toolchain Verification

- Verify toolchain integrity
- Use official distribution sources
- Keep toolchain updated

### Sysroot Security

- Use trusted sysroot sources
- Verify library versions
- Scan for vulnerabilities

## Future Enhancements

### Planned Features

- Additional target architectures
- Docker-based build environments
- Automated sysroot management
- Cross-compilation testing framework

### Integration Opportunities

- IDE integration improvements
- Automated deployment to target hardware
- Performance profiling tools
- Remote debugging support

## Support

### Documentation

- [Phase 1 Build System](PHASE1_BUILD_SYSTEM.md)
- [CMake Presets Documentation](https://cmake.org/cmake/help/latest/manual/cmake-presets.7.html)
- [Cross-Compilation Guide](https://cmake.org/cmake/help/latest/manual/cmake-toolchains.7.html)

### Scripts

- `scripts/setup_cross_compile.sh` - Environment setup
- `scripts/cross_build.sh` - Build automation
- `cmake/toolchains/linux-arm64.cmake` - Toolchain configuration

### Community

- Report issues via project issue tracker
- Contribute improvements via pull requests
- Share sysroot configurations and optimizations