# Building HokuyoHub

**Comprehensive build guide for HokuyoHub - Real-time LiDAR sensor data processing and visualization platform**

This document serves as the main entry point for building HokuyoHub across all supported platforms. HokuyoHub features a sophisticated build system with cross-compilation support, Docker containerization, and flexible dependency management.

## 🚀 Quick Start

**Choose your platform:**

- **[macOS](#macos-native-build)** - Native ARM64 builds using Homebrew
- **[Raspberry Pi 5](#raspberry-pi-5-cross-compilation)** - Cross-compilation from macOS
- **[Docker](#docker-builds)** - Containerized builds for ARM64 Linux

**Fastest path to running HokuyoHub:**

```bash
# macOS (native)
./scripts/build_with_presets.sh release --install

# Raspberry Pi 5 (cross-compile)
./scripts/cross_build.sh --preset rpi-release --install

# Docker (containerized)
./docker/build.sh build-all
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest
```

## 📋 Supported Platforms

| Platform | Architecture | Build Method | Status |
|----------|-------------|--------------|---------|
| macOS Monterey+ | ARM64 | Native | ✅ Primary |
| Raspberry Pi 5 | ARM64 Linux | Cross-compile | ✅ Primary |
| Linux ARM64 | ARM64 | Docker | ✅ Containerized |

## 🛠️ Prerequisites

### All Platforms
- **CMake** 3.18 or newer
- **C++20** compatible compiler
- **Git** for source control

### macOS Specific
```bash
# Install via Homebrew
brew install cmake

# Optional: Install system dependencies
brew install yaml-cpp
```

### Cross-Compilation (Raspberry Pi 5)
```bash
# Set up cross-compilation environment
./scripts/setup_cross_compile.sh

# Or install manually via Homebrew
brew install aarch64-linux-gnu-gcc
```

### Docker
- **Docker Desktop** with BuildKit support
- **Docker Buildx** for multi-platform builds

## 🏗️ Build Methods Overview

### 1. macOS Native Build

**Best for:** Development, testing, and macOS deployment

```bash
# Using CMake presets (recommended)
./scripts/build_with_presets.sh release --install

# Manual CMake
cmake --preset mac-release
cmake --build build/darwin-arm64 --parallel
cmake --install build/darwin-arm64
```

**Output:** [`dist/darwin-arm64/`](dist/darwin-arm64/)

**[📖 Detailed macOS Build Guide](docs/build/macos.md)**

### 2. Raspberry Pi 5 Cross-Compilation

**Best for:** Production deployment on Raspberry Pi 5

```bash
# Setup (one-time)
./scripts/setup_cross_compile.sh

# Build
./scripts/cross_build.sh --preset rpi-release --install
```

**Output:** [`dist/linux-arm64/`](dist/linux-arm64/)

**[📖 Detailed Raspberry Pi Build Guide](docs/build/raspberry-pi.md)**

### 3. Docker Containerized Build

**Best for:** CI/CD, reproducible builds, containerized deployment

```bash
# Build all stages
./docker/build.sh build-all

# Extract artifacts
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest
```

**Output:** [`dist/linux-arm64/`](dist/linux-arm64/)

**[📖 Detailed Docker Build Guide](docs/build/docker.md)**

## ⚙️ CMake Presets

HokuyoHub uses CMake presets for consistent, reproducible builds:

### macOS Presets
- **`mac-debug`** - Debug build with symbols
- **`mac-release`** - Optimized release build
- **`mac-relwithdebinfo`** - Release with debug symbols

### Raspberry Pi 5 Presets
- **`rpi-debug`** - Cross-compiled debug build
- **`rpi-release`** - Cross-compiled release build
- **`rpi-relwithdebinfo`** - Cross-compiled release with debug symbols

### Usage Examples
```bash
# List all available presets
cmake --list-presets

# Configure with preset
cmake --preset mac-release

# Build with preset
cmake --build --preset build-mac-release

# Test with preset
ctest --preset test-mac-release
```

## 📁 Output Structure

All builds follow a consistent platform-specific directory structure:

```
dist/
├── darwin-arm64/           # macOS ARM64 builds
│   ├── hokuyo_hub         # Main executable
│   ├── configs/            # Configuration files
│   ├── webui/             # Web interface
│   └── README.md          # Platform-specific docs
└── linux-arm64/           # Linux ARM64 builds
    ├── hokuyo_hub         # Main executable (ARM64)
    ├── configs/            # Configuration files
    ├── webui/             # Web interface
    └── README.md          # Platform-specific docs
```

## 🔧 Dependency Management

HokuyoHub features flexible dependency management with multiple modes:

### Dependency Modes
```bash
# Automatic (default) - Use system libraries where available
cmake -DDEPS_MODE=auto ..

# System - Force use of system libraries
cmake -DDEPS_MODE=system ..

# Fetch - Build all dependencies from source
cmake -DDEPS_MODE=fetch ..

# Bundled - Use included libraries
cmake -DDEPS_MODE=bundled ..
```

### Key Dependencies
- **CrowCpp** - Lightweight header-only web framework
- **yaml-cpp** - Configuration parsing
- **NNG** - High-performance messaging
- **URG Library** - Hokuyo sensor communication

## 🚦 Common Build Commands

### Quick Commands
```bash
# macOS release build
./scripts/build_with_presets.sh release --install

# Raspberry Pi 5 build
./scripts/cross_build.sh --preset rpi-release --clean --install

# Docker build and extract
./docker/build.sh build-all && ./scripts/extract_docker_artifacts.sh

# Clean all builds
./scripts/build_with_presets.sh clean
```

### Development Workflow
```bash
# 1. Configure for development
cmake --preset mac-debug

# 2. Build incrementally
cmake --build build/darwin-arm64 --target hokuyo_hub

# 3. Install to test
cmake --build build/darwin-arm64 --target install

# 4. Test the application
cd dist/darwin-arm64 && ./hokuyo_hub
```

## 📊 Build Performance

| Build Type | Platform | Time | Output Size |
|------------|----------|------|-------------|
| macOS Release | Native | ~5 min | ~2-5 MB |
| Raspberry Pi Release | Cross-compile | ~8 min | ~2-5 MB |
| Docker Release | Containerized | ~30-45 min | ~2-5 MB |

*Times are approximate and depend on system specifications and available dependencies*

## 🔍 Troubleshooting

### Common Issues

**Build fails with missing dependencies:**
```bash
# Try different dependency mode
cmake -DDEPS_MODE=fetch --preset mac-release
```

**Cross-compilation tools not found:**
```bash
./scripts/setup_cross_compile.sh
```

**Docker build issues:**
```bash
# Clean and retry
./docker/build.sh clean
./docker/build.sh build-all
```

**[📖 Complete Troubleshooting Guide](docs/build/troubleshooting.md)**

## 📚 Detailed Documentation

- **[📖 macOS Build Guide](docs/build/macos.md)** - Complete macOS build instructions
- **[📖 Raspberry Pi Build Guide](docs/build/raspberry-pi.md)** - Cross-compilation setup and build
- **[📖 Docker Build Guide](docs/build/docker.md)** - Containerized builds and deployment
- **[📖 Troubleshooting Guide](docs/build/troubleshooting.md)** - Common issues and solutions
- **[📖 Advanced Configuration](docs/build/advanced.md)** - Custom builds and optimization

## 🎯 Getting Started

New to HokuyoHub? Start here:

1. **[📖 Quick Start Guide](QUICK_START.md)** - Get up and running in minutes
2. Choose your build method from the options above
3. Follow the platform-specific documentation for detailed instructions

## ⚡ Next Steps

After building HokuyoHub:

1. **Configure sensors** - Edit [`configs/default.yaml`](configs/default.yaml)
2. **Start the application** - Run `./hokuyo_hub` from the dist directory
3. **Open web interface** - Navigate to `http://localhost:8080`
4. **Read the documentation** - Check the main [`README.md`](README.md) for usage guide

---

**Build System Version:** 1.0  
**Last Updated:** 2025-08-27  
**Supported Platforms:** macOS ARM64, Raspberry Pi 5 (ARM64 Linux)  
**Build Methods:** Native, Cross-compilation, Docker