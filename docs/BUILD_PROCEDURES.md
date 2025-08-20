# Build Procedures - Hokuyo Hub

## Quick Start

### macOS Build (Recommended)
```bash
# Prerequisites: Homebrew with dependencies installed
brew install drogon yaml-cpp nng

# Build
rm -rf build/darwin-arm64
cmake --preset mac-release -DDEPS_YAMLCPP=system
cmake --build build/darwin-arm64

# Verify
file build/darwin-arm64/hokuyo_hub
# Expected: Mach-O 64-bit executable arm64
```

### Raspberry Pi Cross-Compilation (Toolchain Ready)
```bash
# Prerequisites: Cross-compilation toolchain
brew install musl-cross

# Configure (toolchain validation)
rm -rf build/linux-arm64
cmake --preset rpi-release -DDEPS_MODE=bundled -DHOKUYO_NNG_ENABLE=OFF

# Note: Full build requires complete sysroot with cross-compiled dependencies
# For production: Build natively on Raspberry Pi or use Docker environment
```

## Dependency Management

### DEPS_MODE Options
- `auto` - Intelligent fallback (system → bundled → fetch)
- `system` - Force system packages
- `fetch` - Use FetchContent for remote dependencies  
- `bundled` - Use local bundled dependencies

### Per-Dependency Overrides
- `-DDEPS_DROGON=system|fetch|bundled`
- `-DDEPS_YAMLCPP=system|fetch|bundled`
- `-DDEPS_URG=system|bundled`
- `-DDEPS_NNG=system|fetch`

### Feature Toggles
- `-DHOKUYO_NNG_ENABLE=ON|OFF` - NNG publisher support
- `-DUSE_OSC=ON|OFF` - OSC publisher support

## Platform Status

| Platform | Status | Dependencies | Recommended Mode |
|----------|--------|--------------|------------------|
| macOS | ✅ Full Support | Homebrew | `auto` |
| Raspberry Pi (native) | ✅ Expected Working | apt packages | `auto` |
| Cross-compilation | ⚠️ Toolchain Ready | Requires sysroot | `bundled` |

## Troubleshooting

### macOS Issues
```bash
# yaml-cpp detection issues
cmake --preset mac-release -DDEPS_YAMLCPP=system

# Missing Homebrew dependencies
brew install drogon yaml-cpp nng
```

### Cross-Compilation Issues
```bash
# Verify toolchain
which aarch64-linux-musl-gcc

# Install toolchain
brew install musl-cross

# For full cross-compilation, use Docker or native build
```

## Build Validation

### Successful macOS Build Output
```
-- === Hokuyo Hub Dependency Resolution ===
-- Global DEPS_MODE: auto
-- Cross-compiling: FALSE
-- Auto-selected system Drogon package
-- Using override mode 'system' for yaml-cpp
-- Auto-selected bundled urg_library via ExternalProject
-- Auto-selected system nng library (manual)
-- NNG support enabled
[100%] Built target hokuyo_hub
```

### Binary Verification
```bash
# macOS
file build/darwin-arm64/hokuyo_hub
# Output: Mach-O 64-bit executable arm64

# Linux (when cross-compilation complete)
file build/linux-arm64/hokuyo_hub  
# Expected: ELF 64-bit LSB executable, ARM aarch64
```

For detailed validation results, see [`docs/PHASE3_VALIDATION_REPORT.md`](PHASE3_VALIDATION_REPORT.md).