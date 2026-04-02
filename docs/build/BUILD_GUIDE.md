# Hokuyo Hub Build Guide

## Overview

This guide provides comprehensive instructions for building Hokuyo Hub across different platforms and deployment targets.

## Quick Start

### For Development (macOS)
```bash
# Build via npm
npm run build

# Or run the build script directly
./scripts/build/build_with_presets.sh release --install

# Start (backend is auto-launched, WebUI at http://localhost:8080)
npm start
```

### For Production (Raspberry Pi 5)
```bash
# Docker cross-compilation build for ARM64 Linux
./scripts/build/docker_cross_build.sh --build-all

# Or use GitHub Actions for automated builds (recommended)
# Push to main branch or create version tag
```

## Build Systems

### 1. Native macOS Build

**Purpose**: Development, testing, and debugging on macOS systems.

**Requirements**:
- macOS (tested on macOS Sequoia)
- Homebrew
- CMake 3.25+
- Xcode Command Line Tools

**Build Command**:
```bash
# Via npm
npm run build

# Or directly
./scripts/build/build_with_presets.sh release --install
```

**Output**: `dist/darwin-arm64/hokuyo_hub`

### 2. Docker Cross-Compilation Build

**Purpose**: Production deployment to ARM64 Linux systems (Raspberry Pi 5).

**Requirements**:
- Docker with buildx support
- 4GB+ available disk space

**Build Commands**:
```bash
# Build all stages (recommended)
./scripts/build/docker_cross_build.sh --build-all

# Build specific stages
./scripts/build/docker_cross_build.sh --build-deps      # Dependencies only
./scripts/build/docker_cross_build.sh --build-app      # Application build
./scripts/build/docker_cross_build.sh --build-runtime  # Runtime container
```

**Output**: 
- Docker runtime container: `hokuyo-hub:latest`
- Extracted artifacts: `dist/linux-arm64/`

### 3. Direct Docker Build

**Purpose**: Advanced users who need direct control over Docker build process.

**Build Commands**:
```bash
# Use the comprehensive Docker build script
cd docker/
./build.sh build-all

# Or build individual stages
./build.sh build-deps
./build.sh build-app  
./build.sh build-runtime
```

## Build Configurations

### CMake Presets

The project uses CMake presets for consistent builds:

- **`mac-release`**: Native macOS release build
- **`rpi-release`**: ARM64 Linux release build (used in Docker)
- **`rpi-debug`**: ARM64 Linux debug build
- **`rpi-relwithdebinfo`**: ARM64 Linux release with debug info

### Docker Multi-Stage Build

The Docker build uses a 3-stage approach:

1. **build-deps**: Base dependencies (Debian Bookworm + build tools)
2. **build-app**: Application build with cross-compiled URG library
3. **runtime**: Minimal production container (208MB)

## Build Artifacts

### macOS Native Build
```
dist/darwin-arm64/Release/
├── hokuyo_hub                 # Main executable
├── configs/default.yaml       # Configuration
└── webui/                     # Web interface
```

### ARM64 Linux Build
```
dist/linux-arm64/
├── hokuyo_hub                 # ARM64 executable (2.98MB)
├── configs/default.yaml       # Configuration
├── webui/                     # Web interface (17 files)
├── README.md                  # Deployment instructions
├── MANIFEST.txt               # File inventory
└── CHECKSUMS.txt              # SHA256 verification
```

## Dependencies

### System Dependencies (macOS)
```bash
# Installed via Homebrew
brew install cmake yaml-cpp nng jsoncpp openssl
```

### System Dependencies (Docker/Linux ARM64)
```bash
# Managed automatically in Docker container
libyaml-cpp0.7 libnng1 libssl3 libjsoncpp25 libbrotli1 libuuid1
```

### Third-Party Libraries
- **CrowCpp**: Modern header-only HTTP framework (replaces the previous web framework for faster builds)
- **URG Library**: Hokuyo sensor interface (rebuilt for target platform)
- **yaml-cpp**: YAML configuration parsing
- **NNG**: High-performance messaging library

## Troubleshooting

For build and deployment troubleshooting, see [トラブルシューティング](トラブルシューティング.md) and [`DOCKER_BUILD_DEBUG_REPORT.md`](DOCKER_BUILD_DEBUG_REPORT.md).

## Advanced Usage

### Custom Docker Build

```bash
# Build with specific Dockerfile
docker buildx build --platform linux/arm64 -f docker/Dockerfile.rpi -t my-hokuyo-hub .

# Build with custom build args
docker buildx build --build-arg CMAKE_BUILD_TYPE=Debug --platform linux/arm64 -f docker/Dockerfile.rpi .
```

### Environment Setup

```bash
# Set up cross-compilation environment (if needed)
./scripts/setup/setup_cross_compile.sh
```

### Testing

```bash
# Run API tests (requires running server)
./scripts/testing/test_rest_api.sh http://localhost:8080
```

## Deployment

### Raspberry Pi 5 Deployment

1. **Copy distribution**:
   ```bash
   # Copy dist/linux-arm64/ to target system
   scp -r dist/linux-arm64/ pi@raspberrypi:/opt/hokuyohub/
   ```

2. **Install runtime dependencies**:
   ```bash
   # On Raspberry Pi
   sudo apt update
   sudo apt install libyaml-cpp0.7 libnng1 libssl3 libjsoncpp25
   ```

3. **Run application**:
   ```bash
   cd /opt/hokuyohub
   chmod +x hokuyo_hub
   ./hokuyo_hub
   ```

### Docker Container Deployment

```bash
# Run the built container
docker run -p 8080:8080 -p 8081:8081 hokuyo-hub:latest
```

## Related Documentation

- [ビルドガイド（日本語）](ビルドガイド.md) - Detailed build guide with performance data
- [`DOCKER_BUILD_DEBUG_REPORT.md`](DOCKER_BUILD_DEBUG_REPORT.md) - Docker build debugging
- [`../scripts/README.md`](../scripts/README.md) - Scripts documentation