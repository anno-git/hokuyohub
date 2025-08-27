# Phase 1: CMake Presets Build System

## Overview

Phase 1 introduces CMake presets to the HokuyoHub build system, providing a foundation for cross-compilation while maintaining complete backward compatibility with existing workflows.

## Key Features

- **Backward Compatibility**: All existing build commands continue to work unchanged
- **CMake Presets**: Standardized build configurations using CMake 3.18+ presets
- **Convenience Scripts**: Optional wrapper scripts for easier preset usage
- **Foundation for Cross-Compilation**: Prepared structure for Phase 2 cross-compilation
- **Comprehensive Testing**: Integration with existing Phase 0 safety nets

## Build Methods

### Method 1: Traditional CMake (Unchanged)

```bash
# This continues to work exactly as before
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NNG=ON ..
make -j$(nproc)
make install
```

### Method 2: CMake Presets (New - Recommended)

```bash
# Configure and build in one step
cmake --preset mac-release
cmake --build --preset build-mac-release
cmake --install build/darwin-arm64

# Or step by step
cmake --preset mac-debug                    # Configure
cmake --build --preset build-mac-debug      # Build
cmake --install build/darwin-arm64          # Install
```

### Method 3: Convenience Scripts (New)

```bash
# Full build script with options
./scripts/build_with_presets.sh release --install --verbose

# Simple wrapper
./scripts/preset_build.sh release

# Clean build directories
./scripts/build_with_presets.sh clean
```

## Available Presets

### Configure Presets

| Preset | Description | Build Type | Use Case |
|--------|-------------|------------|----------|
| `mac-base` | Base macOS configuration | - | Inheritance base |
| `mac-debug` | Debug build | Debug | Development |
| `mac-release` | Release build | Release | Production |
| `mac-relwithdebinfo` | Release with debug info | RelWithDebInfo | Profiling |
| `rpi-base` | Raspberry Pi cross-compile | Release | Future Phase 2 |

### Build Presets

| Preset | Description | Targets |
|--------|-------------|---------|
| `build-mac-debug` | Build Debug | hokuyo_hub |
| `build-mac-release` | Build Release | hokuyo_hub |
| `build-mac-relwithdebinfo` | Build RelWithDebInfo | hokuyo_hub |
| `build-all-mac` | Build all targets | all |
| `build-linux-arm64` | Cross-compile ARM64 | hokuyo_hub |

### Test Presets

| Preset | Description |
|--------|-------------|
| `test-mac-debug` | Run tests for Debug build |
| `test-mac-release` | Run tests for Release build |

### Package Presets

| Preset | Description |
|--------|-------------|
| `package-mac-release` | Create distribution package |

## Directory Structure

Phase 1 uses a nested directory structure to prepare for cross-compilation:

```
build/
├── darwin-arm64/          # macOS ARM64 builds
│   ├── CMakeCache.txt
│   ├── hokuyo_hub         # Built executable
│   └── ...
└── linux-arm64/           # Future: Linux ARM64 cross-builds
    └── ...

dist/
├── darwin-arm64/          # macOS ARM64 distribution
│   ├── hokuyo_hub         # Installed executable
│   ├── config/            # Configuration files
│   └── webui/             # Web interface
└── linux-arm64/           # Future: Linux ARM64 distribution
    └── ...
```

## Configuration Details

### Cache Variables

All presets include these standard cache variables:

```cmake
CMAKE_PREFIX_PATH=/opt/homebrew          # Homebrew dependencies
ENABLE_NNG=ON                           # Enable NNG support
USE_OSC=ON                              # Enable OSC support
USE_NNG=OFF                             # Disable NNG (conflicts with ENABLE_NNG)
CMAKE_EXPORT_COMPILE_COMMANDS=ON        # IDE integration
CMAKE_INSTALL_PREFIX=${sourceDir}/dist/darwin-arm64
```

### Environment Variables

```bash
PKG_CONFIG_PATH=/opt/homebrew/lib/pkgconfig
```

## Validation and Testing

### Regression Testing

Phase 1 integrates with existing Phase 0 safety nets:

```bash
# Run baseline verification with presets
./scripts/baseline_verification.sh --compare-baseline

# Run comprehensive regression check
./scripts/regression_check.sh --full

# Quick regression check
./scripts/regression_check.sh --quick
```

### Manual Validation

```bash
# Verify presets produce identical results
cmake --preset mac-release
cmake --build --preset build-mac-release
cmake --install build/darwin-arm64

# Compare with traditional build
mkdir -p build-traditional && cd build-traditional
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NNG=ON ..
make -j$(nproc)
make install DESTDIR=../dist-traditional

# Binary should be identical (size, dependencies, functionality)
```

## Migration Guide

### For Developers

1. **No immediate action required** - existing workflows continue to work
2. **Optional**: Start using presets for new development
3. **Recommended**: Use convenience scripts for common operations

### For CI/CD

```bash
# Replace this:
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release -DENABLE_NNG=ON ..
make -j$(nproc)
make install

# With this:
cmake --preset mac-release
cmake --build --preset build-mac-release
cmake --install build/darwin-arm64

# Or use convenience script:
./scripts/build_with_presets.sh release --install
```

### For Scripts

```bash
# Old way (still works):
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make

# New way (recommended):
./scripts/preset_build.sh release

# Or direct preset usage:
cmake --preset mac-release
cmake --build --preset build-mac-release
```

## Troubleshooting

### Common Issues

1. **CMake version too old**
   ```bash
   # Requires CMake 3.18+
   cmake --version
   brew upgrade cmake  # macOS
   ```

2. **Preset not found**
   ```bash
   # Verify presets are available
   cmake --list-presets
   ```

3. **Build directory conflicts**
   ```bash
   # Clean and rebuild
   ./scripts/build_with_presets.sh clean
   ./scripts/build_with_presets.sh release --install
   ```

### Debugging

```bash
# Verbose preset build
./scripts/build_with_presets.sh release --verbose

# Check preset configuration
cmake --preset mac-release --debug-output

# Verify cache variables
grep -E "(CMAKE_|ENABLE_|USE_)" build/darwin-arm64/CMakeCache.txt
```

## Phase 2 Preparation

Phase 1 establishes the foundation for Phase 2 cross-compilation:

- **Directory structure** ready for multiple target platforms
- **Preset inheritance** allows easy addition of cross-compilation presets
- **Toolchain integration** prepared with `rpi-base` placeholder
- **Testing framework** ready for multi-platform validation

## Benefits

1. **Consistency**: Standardized build configurations across environments
2. **Reproducibility**: Identical builds from identical presets
3. **Flexibility**: Multiple build types and configurations available
4. **IDE Integration**: Better support for modern IDEs with preset awareness
5. **Future-Proof**: Foundation for cross-compilation and CI/CD improvements

## Next Steps

- **Phase 2**: Implement full cross-compilation support
- **CI/CD Integration**: Migrate build pipelines to use presets
- **IDE Configuration**: Provide preset-aware IDE configurations
- **Documentation**: Expand cross-compilation documentation