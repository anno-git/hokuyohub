# macOS Build Guide

**Detailed instructions for building HokuyoHub on macOS**

This guide covers native macOS builds using Apple Silicon (ARM64) architecture. For cross-compilation to other platforms, see the [main BUILD.md](../../BUILD.md).

## 🌟 CrowCpp Migration Benefits

HokuyoHub has migrated from Drogon to **CrowCpp** for improved macOS build experience:

- **⚡ Faster Builds**: No complex web framework compilation required
- **🪶 Simplified Setup**: Header-only design eliminates brew install dependencies
- **🔧 Better Development**: Faster incremental builds and cleaner error messages
- **📈 Improved Performance**: Lower memory usage and faster startup times

## � Quick Start

```bash
# Install prerequisites
brew install cmake

# Build and install
./scripts/build_with_presets.sh release --install

# Run HokuyoHub
cd dist/darwin-arm64 && ./hokuyo_hub
```

## 📋 Prerequisites

### System Requirements
- **macOS** Monterey (12.0) or newer
- **Architecture** Apple Silicon (ARM64) or Intel (x86_64)
- **Xcode Command Line Tools** or Xcode
- **Homebrew** package manager

### Install Prerequisites

**1. Install Xcode Command Line Tools:**
```bash
xcode-select --install
```

**2. Install Homebrew (if not already installed):**
```bash
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"
```

**3. Install CMake:**
```bash
brew install cmake

# Verify installation
cmake --version  # Should be 3.18+
```

**4. Optional - Install System Dependencies:**
```bash
# Install system libraries (optional, HokuyoHub can build its own)
brew install yaml-cpp

# Note: CrowCpp is header-only and requires no installation
# (replaces Drogon for faster builds and simplified dependencies)

# For development
brew install pkg-config
```

## 🛠️ Build Methods

### Method 1: CMake Presets (Recommended)

HokuyoHub uses CMake presets for consistent, reproducible builds:

**Available Presets:**
- `mac-debug` - Debug build with symbols and verbose output
- `mac-release` - Optimized release build (recommended)
- `mac-relwithdebinfo` - Release build with debug symbols

**Build Commands:**
```bash
# Configure with preset
cmake --preset mac-release

# Build
cmake --build build/darwin-arm64 --parallel

# Install to dist directory
cmake --install build/darwin-arm64
```

### Method 2: Build Script (Easiest)

Use the provided script for common build configurations:

```bash
# Build release (default)
./scripts/build_with_presets.sh release --install

# Build debug
./scripts/build_with_presets.sh debug --install

# Build with tests
./scripts/build_with_presets.sh release --install --test

# Clean build directories
./scripts/build_with_presets.sh clean
```

**Script Options:**
- `--install` - Install to dist directory after build
- `--test` - Run tests after build (if available)
- `--package` - Create distribution package
- `--verbose` - Enable verbose output

### Method 3: Manual CMake

For complete control over the build process:

```bash
# Create build directory
mkdir -p build/darwin-arm64
cd build/darwin-arm64

# Configure
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=../../dist/darwin-arm64 \
  -DCMAKE_PREFIX_PATH=/opt/homebrew \
  -DHOKUYO_NNG_ENABLE=ON \
  -DUSE_OSC=ON \
  ../..

# Build
cmake --build . --parallel $(sysctl -n hw.ncpu)

# Install
cmake --build . --target install
```

## ⚙️ Configuration Options

### Dependency Management

HokuyoHub supports multiple dependency modes:

```bash
# Automatic mode (default) - use system libs where available
cmake -DDEPS_MODE=auto --preset mac-release

# System mode - force use of system libraries
cmake -DDEPS_MODE=system --preset mac-release

# Fetch mode - build all dependencies from source
cmake -DDEPS_MODE=fetch --preset mac-release

# Bundled mode - use included dependencies
cmake -DDEPS_MODE=bundled --preset mac-release
```

### Feature Toggles

```bash
# Enable/disable features during configure
cmake --preset mac-release \
  -DHOKUYO_NNG_ENABLE=ON \      # NNG publisher support
  -DUSE_OSC=ON \                # OSC protocol support
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON  # For IDE integration
```

### Optimization Levels

```bash
# Debug build (for development)
cmake --preset mac-debug

# Release build (for production)
cmake --preset mac-release

# Release with debug info (for profiling)
cmake --preset mac-relwithdebinfo
```

## 📁 Output Structure

All builds output to the `dist/darwin-arm64/` directory:

```
dist/darwin-arm64/
├── hokuyo_hub                  # Main executable
├── config/                     # Configuration files
│   ├── default.yaml           # Default configuration
│   └── examples/              # Example configurations
├── webui/                     # Web interface files
│   ├── index.html
│   ├── styles.css
│   └── js/                    # JavaScript files
└── README.md                  # Platform-specific documentation
```

## 🔧 Development Workflow

### Setting up Development Environment

**1. Configure for development:**
```bash
# Use debug preset for development
cmake --preset mac-debug
```

**2. Build specific targets:**
```bash
# Build only the main executable
cmake --build build/darwin-arm64 --target hokuyo_hub

# Build everything
cmake --build build/darwin-arm64
```

**3. Incremental builds:**
```bash
# After making changes, rebuild quickly
cmake --build build/darwin-arm64 --parallel
```

### IDE Integration

**VS Code:**
```bash
# Generate compile_commands.json
cmake --preset mac-debug -DCMAKE_EXPORT_COMPILE_COMMANDS=ON

# Copy to project root for IDE
cp build/darwin-arm64/compile_commands.json .
```

**Xcode:**
```bash
# Generate Xcode project
cmake -G Xcode --preset mac-debug
open build/darwin-arm64/HokuyoHub.xcodeproj
```

### Debugging

**1. Build debug version:**
```bash
cmake --preset mac-debug
cmake --build build/darwin-arm64
cmake --install build/darwin-arm64
```

**2. Run with debugger:**
```bash
cd dist/darwin-arm64
lldb ./hokuyo_hub

# In lldb:
(lldb) run --config ./config/default.yaml --listen 0.0.0.0:8080
```

**3. Debug with VS Code:**
Install C++ extension and create `.vscode/launch.json`:
```json
{
  "version": "0.2.0",
  "configurations": [
    {
      "name": "Debug HokuyoHub",
      "type": "lldb",
      "request": "launch",
      "program": "${workspaceFolder}/dist/darwin-arm64/hokuyo_hub",
      "args": ["--config", "./config/default.yaml", "--listen", "0.0.0.0:8080"],
      "cwd": "${workspaceFolder}/dist/darwin-arm64",
      "console": "integratedTerminal"
    }
  ]
}
```

## 📊 Performance Optimization

### Build Performance

**Parallel Builds:**
```bash
# Use all CPU cores
cmake --build build/darwin-arm64 --parallel $(sysctl -n hw.ncpu)

# Or specify core count
cmake --build build/darwin-arm64 --parallel 8
```

**Ccache (Compiler Cache):**
```bash
# Install ccache
brew install ccache

# Configure CMake to use ccache
cmake --preset mac-release -DCMAKE_CXX_COMPILER_LAUNCHER=ccache
```

**Ninja Build System:**
```bash
# Install Ninja (faster than Make)
brew install ninja

# Use Ninja generator
cmake --preset mac-release -G Ninja
ninja -C build/darwin-arm64
```

### Runtime Performance

**Release Build Optimizations:**
- Uses `-O3` optimization level
- Enables all CPU-specific optimizations for Apple Silicon
- Strips debug symbols for smaller binary size
- Uses Link Time Optimization (LTO) when available

**Memory Usage:**
```bash
# Monitor memory usage during build
top -pid $(pgrep cmake)

# Profile application memory
instruments -t "Allocations" ./hokuyo_hub
```

## 🔍 Troubleshooting

### Common Build Issues

**1. CMake Version Too Old:**
```bash
# Error: CMake 3.18 or higher is required
brew upgrade cmake
cmake --version
```

**2. Xcode Command Line Tools Missing:**
```bash
# Error: No CMAKE_C_COMPILER could be found
xcode-select --install
```

**3. Homebrew Dependencies Not Found:**
```bash
# Error: Could not find yaml-cpp
export CMAKE_PREFIX_PATH=/opt/homebrew:$CMAKE_PREFIX_PATH

# Or force fetch mode
cmake -DDEPS_MODE=fetch --preset mac-release
```

**4. Permission Issues:**
```bash
# Error: Permission denied
sudo chown -R $(whoami) /opt/homebrew
```

### Dependency Issues

**Missing System Libraries:**
```bash
# Install missing dependencies
brew install yaml-cpp libnng openssl zlib

# Note: CrowCpp is header-only and requires no system installation
# Or use fetch mode to build from source
cmake -DDEPS_MODE=fetch --preset mac-release
```

**Conflicting Library Versions:**
```bash
# Clean and rebuild with specific mode
./scripts/build_with_presets.sh clean
cmake -DDEPS_MODE=system --preset mac-release
```

### Runtime Issues

**Binary Won't Execute:**
```bash
# Check architecture
file dist/darwin-arm64/hokuyo_hub

# Check dynamic library dependencies
otool -L dist/darwin-arm64/hokuyo_hub

# Fix library paths if needed
install_name_tool -change old_path new_path dist/darwin-arm64/hokuyo_hub
```

**Port Already in Use:**
```bash
# Find process using port 8080
lsof -i :8080

# Kill process if needed
kill -9 $(lsof -t -i:8080)

# Use different port
./hokuyo_hub --listen 0.0.0.0:8081
```

## 🧪 Testing

### Build Tests

```bash
# Build with tests enabled
./scripts/build_with_presets.sh debug --test

# Run tests manually
cd build/darwin-arm64
ctest --verbose
```

### Integration Testing

```bash
# Test REST API
./scripts/test_rest_api.sh

# Manual testing
cd dist/darwin-arm64
./hokuyo_hub --config ./config/default.yaml &
curl http://localhost:8080/api/health
```

## 📦 Distribution

### Creating Packages

```bash
# Build release package
./scripts/build_with_presets.sh release --package

# Manual packaging
cd build/darwin-arm64
cpack
```

### Deployment

```bash
# Create distributable archive
cd dist
tar -czf hokuyo-hub-darwin-arm64.tar.gz darwin-arm64/

# Or create DMG (requires additional setup)
hdiutil create -volname "HokuyoHub" -srcfolder darwin-arm64 HokuyoHub.dmg
```

## 🎯 Advanced Configuration

### Custom Install Locations

```bash
# Install to custom directory
cmake --preset mac-release -DCMAKE_INSTALL_PREFIX=/usr/local/hokuyo-hub
cmake --build build/darwin-arm64 --target install
```

### Static Linking

```bash
# Build with static dependencies (where possible)
cmake --preset mac-release -DBUILD_SHARED_LIBS=OFF
```

### Universal Binaries (x86_64 + ARM64)

```bash
# Build universal binary (requires Xcode)
cmake -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64" --preset mac-release
```

## 📚 Additional Resources

- **[Main Build Guide](../../BUILD.md)** - Overview of all build methods
- **[Quick Start Guide](../../QUICK_START.md)** - Get running quickly
- **[Troubleshooting Guide](troubleshooting.md)** - Common issues and solutions
- **[Advanced Configuration](advanced.md)** - Expert-level customization

---

**macOS Build Guide Version:** 1.0  
**Last Updated:** 2025-08-27  
**Compatible Systems:** macOS Monterey+, Apple Silicon + Intel  
**Build System:** CMake 3.18+ with presets