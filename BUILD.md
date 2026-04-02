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
./scripts/build/build_with_presets.sh release --install

# Frontend dev server (proxies to backend on port 8081)
cd webui-server && npm start
# Web UI: http://localhost:3000

# Raspberry Pi 5 (cross-compile)
./scripts/build/docker_cross_build.sh --build-all

# Docker (containerized) - OPTIMIZED
./docker/build.sh build-all  # Uses Dockerfile.rpi.optimized
./scripts/utils/extract_docker_artifacts.sh hokuyo-hub:latest
```

### 🚀 Performance Improvements
The optimized build system delivers **60-70% faster builds**:
- **Before**: 25-35 minutes
- **After**: 8-12 minutes
- **Key optimizations**: Matrix parallelization, BuildKit caching, dependency pre-building

## � Supported Platforms

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
./scripts/setup/setup_cross_compile.sh

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
./scripts/build/build_with_presets.sh release --install

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
./scripts/setup/setup_cross_compile.sh

# Build (Docker-based cross-compilation)
./scripts/build/docker_cross_build.sh --build-all
```

**Output:** [`dist/linux-arm64/`](dist/linux-arm64/)

**[📖 Detailed Raspberry Pi Build Guide](docs/build/raspberry-pi.md)**

### 3. Docker Containerized Build

**Best for:** CI/CD, reproducible builds, containerized deployment

```bash
# Build all stages
./docker/build.sh build-all

# Extract artifacts
./scripts/utils/extract_docker_artifacts.sh hokuyo-hub:latest
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
│   └── configs/            # Configuration files
└── linux-arm64/           # Linux ARM64 builds
    ├── hokuyo_hub         # Main executable (ARM64)
    └── configs/            # Configuration files
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

## 📊 Build Performance

| Build Type | Platform | Before | After | Improvement |
|------------|----------|--------|--------|-------------|
| macOS Release | Native | ~8 min | ~5 min | 37% faster |
| Raspberry Pi Release | Cross-compile | ~15 min | ~8 min | 47% faster |
| **Docker Release** | **Containerized** | **~30-45 min** | **~8-12 min** | **🚀 60-70% faster** |

### Key Performance Optimizations
- **Matrix Parallelization**: Multi-platform builds run simultaneously
- **BuildKit Layer Caching**: Persistent Docker layer caching
- **URG Library Pre-building**: Dependency caching eliminates rebuild time
- **Advanced Mount Caches**: APT packages, ccache, build directories
- **GitHub Actions Integration**: Optimized CI/CD pipeline

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

詳細: **[docs/build/troubleshooting.md](docs/build/troubleshooting.md)**

## 📚 関連ドキュメント

- **[QUICK_START.md](QUICK_START.md)** — 最速で動かす手順
- **[README.md](README.md)** — 機能詳細、設定、REST API
- **[docs/build/](docs/build/)** — プラットフォーム別ビルドガイド (macOS, Raspberry Pi, Docker, Advanced)