# Docker Build System for Hokuyo Hub

## Overview

This directory contains the Docker-based build system for Hokuyo Hub targeting ARM64 Linux (Raspberry Pi 5). The build system follows the established platform-specific directory structure pattern.

## Directory Structure

```
docker/
├── build.sh                    # Main build script with multiple commands
├── docker-compose.yml          # Docker Compose configuration for testing
├── Dockerfile.rpi              # Multi-stage Dockerfile for ARM64 Linux
├── Dockerfile.rpi.fixed        # Fixed version (backup)
├── Dockerfile.rpi.rebuild      # Version with URG library rebuild
└── README.md                   # This documentation
```

## Platform-Specific Output Structure

The Docker build system outputs artifacts to maintain consistency with the established pattern:

- **macOS builds**: `dist/darwin-arm64/`
- **Docker builds**: `dist/linux-arm64/` (updated from `dist/`)

## Quick Start

### 1. Build All Stages
```bash
./docker/build.sh build-all
```

### 2. Extract Artifacts
```bash
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest
```

### 3. Verify Output Structure
```bash
ls -la dist/linux-arm64/
# Expected:
# - hokuyo_hub (ARM64 Linux binary)
# - config/ (configuration files)
# - webui/ (web interface files)
# - README.md (distribution documentation)
```

## Build Commands

### Available Commands

| Command | Description |
|---------|-------------|
| `build-deps` | Build only the dependencies stage |
| `build-app` | Build the application (includes dependencies) |
| `build-runtime` | Build the complete runtime image |
| `build-all` | Build all stages |
| `test-build` | Test the build process |
| `test-run` | Test running the container |
| `clean` | Clean up Docker images and containers |
| `help` | Show help message |

### Environment Variables

| Variable | Default | Description |
|----------|---------|-------------|
| `DOCKER_REGISTRY` | - | Docker registry prefix (optional) |
| `IMAGE_NAME` | `hokuyo-hub` | Image name |
| `IMAGE_TAG` | `latest` | Image tag |
| `PLATFORM` | `linux/arm64` | Target platform |
| `DOCKERFILE` | `docker/Dockerfile.rpi` | Dockerfile path |

### Examples

```bash
# Standard build
./docker/build.sh build-all

# Test build process
./docker/build.sh test-build

# Build with custom tag
IMAGE_TAG=v1.0.0 ./docker/build.sh build-runtime

# Build for different platform
PLATFORM=linux/amd64 ./docker/build.sh build-all
```

## Docker Compose Usage

### Services Available

| Service | Purpose | Target Stage |
|---------|---------|--------------|
| `hokuyo-hub-build` | Build testing | `build-app` |
| `hokuyo-hub` | Runtime testing | `runtime` |
| `hokuyo-hub-dev` | Development | `runtime` |

### Usage Examples

```bash
# Build and test
docker-compose -f docker/docker-compose.yml build
docker-compose -f docker/docker-compose.yml up

# Build only (for testing build process)
docker-compose -f docker/docker-compose.yml build hokuyo-hub-build

# Run with custom config
docker-compose -f docker/docker-compose.yml up -d hokuyo-hub
```

## Multi-Stage Build Process

### Stage 1: Build Dependencies (`build-deps`)
- Base: Debian Bookworm Slim
- Installs build tools and system dependencies
- Prepares environment for compilation

### Stage 2: Application Build (`build-app`)
- Copies source code
- Configures CMake with platform-specific settings
- Builds application and dependencies
- Installs to staging area (`/staging/opt/hokuyohub`)

### Stage 3: Runtime Image (`runtime`)
- Minimal runtime dependencies only
- Copies application from build stage
- Sets up application user and permissions
- Configures health checks

## Artifact Extraction

### Automatic Extraction
Use the dedicated extraction script:
```bash
./scripts/extract_docker_artifacts.sh [image_name]
```

### Manual Extraction
```bash
# Create temporary container
docker create --name temp-extract --platform linux/arm64 hokuyo-hub:latest

# Extract to platform-specific directory
mkdir -p dist/linux-arm64
docker cp temp-extract:/opt/hokuyohub/ dist/linux-arm64/

# Cleanup
docker rm temp-extract
```

### Output Structure
```
dist/linux-arm64/
├── hokuyo_hub              # Main ARM64 binary
├── config/                 # Configuration files
│   └── default.yaml
├── webui/                  # Web interface files
│   ├── index.html
│   ├── styles.css
│   └── js/
├── README.md               # Distribution documentation
├── MANIFEST.txt            # File listing
└── CHECKSUMS.txt           # Binary verification
```

## Build Configuration

### CMake Configuration
```cmake
CMAKE_BUILD_TYPE=Release
CMAKE_INSTALL_PREFIX=/opt/hokuyohub
DEPS_MODE=auto
DEPS_DROGON=fetch          # Build from source
DEPS_YAMLCPP=system        # Use Debian package
DEPS_NNG=system            # Use Debian package
DEPS_URG=bundled           # Rebuild from source
HOKUYO_NNG_ENABLE=ON       # Enable NNG publisher
USE_OSC=ON                 # Enable OSC support
```

### Platform Targeting
- **Target Platform**: `linux/arm64`
- **Target Hardware**: Raspberry Pi 5 (Cortex-A76)
- **Base Image**: Debian Bookworm Slim
- **Architecture**: ARM64/aarch64

## Troubleshooting

### Common Issues

#### 1. Build Failures
```bash
# Clean Docker buildx cache
docker buildx prune -f

# Rebuild from scratch
./docker/build.sh clean
./docker/build.sh build-all
```

#### 2. Architecture Mismatch
```bash
# Verify binary architecture
file dist/linux-arm64/hokuyo_hub
# Expected: ELF 64-bit LSB pie executable, ARM aarch64
```

#### 3. Missing Dependencies
```bash
# Check runtime dependencies
ldd dist/linux-arm64/hokuyo_hub
# All libraries should be found
```

### Build Performance

| Metric | Typical Value |
|--------|---------------|
| Total Build Time | 30-45 minutes |
| URG Library Rebuild | ~2 minutes |
| Main Application | ~5 minutes |
| With Layer Caching | ~10 minutes |
| Build Memory Usage | ~4GB peak |
| Final Binary Size | ~2-5MB |

## Integration with CI/CD

### GitHub Actions Example
```yaml
- name: Build Docker Image
  run: ./docker/build.sh build-all

- name: Extract Artifacts
  run: ./scripts/extract_docker_artifacts.sh hokuyo-hub:latest

- name: Upload Artifacts
  uses: actions/upload-artifact@v3
  with:
    name: hokuyo-hub-linux-arm64
    path: dist/linux-arm64/
```

### Validation Steps
```bash
# 1. Build verification
./docker/build.sh test-build

# 2. Runtime verification
./docker/build.sh test-run

# 3. Artifact extraction
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest

# 4. Binary verification
file dist/linux-arm64/hokuyo_hub
```

## Deployment

### Target System Requirements
- **OS**: ARM64 Linux (Debian/Ubuntu based)
- **Hardware**: Raspberry Pi 5 or compatible ARM64 system
- **Memory**: Minimum 1GB RAM recommended
- **Storage**: 100MB for application and assets

### Installation Process
```bash
# 1. Copy platform-specific artifacts
scp -r dist/linux-arm64/ pi@raspberrypi:~/hokuyo-hub/

# 2. Install runtime dependencies
sudo apt update
sudo apt install libyaml-cpp0.7 libnng1 libssl3 zlib1g \
                 libjsoncpp25 libbrotli1 libuuid1

# 3. Run application
cd ~/hokuyo-hub
chmod +x hokuyo_hub
./hokuyo_hub
```

## Maintenance

### Regular Tasks
- Monitor build performance and optimize as needed
- Update base images for security patches
- Validate cross-platform compatibility
- Update documentation for any changes

### Version Management
- Tag Docker images with semantic versions
- Maintain compatibility with existing deployment scripts
- Document breaking changes in build process

---

**Last Updated**: 2025-08-20  
**Target Platform**: linux/arm64 (Raspberry Pi 5)  
**Build System**: Docker Multi-stage with Debian Bookworm  
**Output Structure**: `dist/linux-arm64/` (platform-specific)