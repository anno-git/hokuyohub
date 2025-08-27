# Docker Build Guide

**Containerized builds for HokuyoHub targeting ARM64 Linux (Raspberry Pi 5)**

This guide covers Docker-based builds using multi-stage containers for reproducible, portable ARM64 Linux binaries. The Docker system provides an alternative to cross-compilation for targeting Raspberry Pi 5 systems.

## 🚀 Quick Start

```bash
# Build all stages
./docker/build.sh build-all

# Extract artifacts to dist/linux-arm64/
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest

# Run locally (if compatible)
cd dist/linux-arm64 && ./hokuyo_hub
```

## 🐳 Docker Build System Overview

### Multi-Stage Architecture

The Docker build system uses a sophisticated multi-stage approach:

1. **`build-deps`** - Install build tools and system dependencies
2. **`build-app`** - Compile HokuyoHub and all dependencies  
3. **`runtime`** - Create minimal runtime image with only essentials

### Platform Targeting

- **Target Platform:** `linux/arm64`
- **Target Hardware:** Raspberry Pi 5 (Cortex-A76)
- **Base Image:** Debian Bookworm Slim
- **Output:** ARM64 Linux executable + assets

## 📋 Prerequisites

### System Requirements
- **Docker Desktop** or Docker Engine 20.10+
- **Docker Buildx** for multi-platform builds
- **8GB+ RAM** recommended for build process
- **20GB+ free disk space** for build cache and images

### Installation Verification

```bash
# Check Docker version
docker version
# Client and Server should be 20.10+

# Check Buildx availability
docker buildx version
# Should show buildx version

# Verify multi-platform support
docker buildx ls
# Should show linux/arm64 in supported platforms
```

## 🛠️ Build Methods

### Method 1: Build Script (Recommended)

The [`docker/build.sh`](../../docker/build.sh) script provides convenient commands for all Docker operations:

**Available Commands:**
- `build-deps` - Build only dependencies stage
- `build-app` - Build application stage (includes deps)
- `build-runtime` - Build complete runtime image
- `build-all` - Build all stages (recommended)
- `test-build` - Test build process without runtime
- `test-run` - Test running the container
- `clean` - Clean up images and containers

**Usage Examples:**
```bash
# Complete build process
./docker/build.sh build-all

# Test build without creating runtime
./docker/build.sh test-build

# Build only application stage for testing
./docker/build.sh build-app

# Clean up everything
./docker/build.sh clean
```

### Method 2: Docker Buildx Commands

For direct control over the build process:

```bash
# Build all stages to completion
docker buildx build \
    --platform linux/arm64 \
    --target runtime \
    --tag hokuyo-hub:latest \
    --file docker/Dockerfile.rpi \
    --load \
    .

# Build specific stage
docker buildx build \
    --platform linux/arm64 \
    --target build-app \
    --tag hokuyo-hub:build \
    --file docker/Dockerfile.rpi \
    .

# Build with progress output
docker buildx build \
    --platform linux/arm64 \
    --target runtime \
    --tag hokuyo-hub:latest \
    --file docker/Dockerfile.rpi \
    --progress=plain \
    --load \
    .
```

### Method 3: Docker Compose

Use the provided Docker Compose configuration for testing:

```bash
# Build and run all services
docker-compose -f docker/docker-compose.yml build
docker-compose -f docker/docker-compose.yml up

# Build specific service
docker-compose -f docker/docker-compose.yml build hokuyo-hub-build

# Run in background
docker-compose -f docker/docker-compose.yml up -d hokuyo-hub
```

## ⚙️ Build Configuration

### Environment Variables

Configure builds using environment variables:

| Variable | Default | Description |
|----------|---------|-------------|
| `DOCKER_REGISTRY` | - | Registry prefix (optional) |
| `IMAGE_NAME` | `hokuyo-hub` | Docker image name |
| `IMAGE_TAG` | `latest` | Image tag |
| `PLATFORM` | `linux/arm64` | Target platform |
| `DOCKERFILE` | `docker/Dockerfile.rpi` | Dockerfile path |

**Usage Examples:**
```bash
# Build with custom tag
IMAGE_TAG=v1.0.0 ./docker/build.sh build-all

# Build for different platform (testing)
PLATFORM=linux/amd64 ./docker/build.sh build-all

# Use different dockerfile
DOCKERFILE=docker/Dockerfile.rpi.fixed ./docker/build.sh build-all
```

### Build Arguments

The Dockerfile supports build-time arguments for dependency management:

```bash
# Build with specific dependency modes
docker buildx build \
    --platform linux/arm64 \
    --build-arg DEPS_MODE=system \
    --build-arg HOKUYO_NNG_ENABLE=ON \
    --build-arg USE_OSC=ON \
    --target runtime \
    --tag hokuyo-hub:custom \
    --file docker/Dockerfile.rpi \
    .
```

**Available Build Args:**
- `DEPS_MODE` - Dependency management mode (`auto`, `system`, `fetch`, `bundled`)
- `HOKUYO_NNG_ENABLE` - Enable NNG publisher (`ON`/`OFF`)
- `USE_OSC` - Enable OSC protocol support (`ON`/`OFF`)

## 📁 Multi-Stage Build Details

### Stage 1: Build Dependencies (`build-deps`)

**Purpose:** Prepare build environment with all required tools

**Contents:**
- Debian Bookworm Slim base image
- Build tools (gcc, cmake, make, git)
- System development libraries
- Package manager setup

**Dockerfile Section:**
```dockerfile
FROM --platform=$TARGETPLATFORM debian:bookworm-slim AS build-deps
# Install build essentials and development libraries
RUN apt-get update && apt-get install -y \
    build-essential cmake git pkg-config \
    libyaml-cpp-dev libnng-dev libssl-dev \
    && rm -rf /var/lib/apt/lists/*
```

### Stage 2: Application Build (`build-app`)

**Purpose:** Compile HokuyoHub and all dependencies

**Contents:**
- Source code compilation
- External dependency builds (Drogon, URG library)
- CMake configuration and build
- Staging area preparation

**Key Features:**
- ARM64 optimizations for Raspberry Pi 5
- Parallel compilation for faster builds  
- Dependency caching for rebuild efficiency
- Binary validation and testing

### Stage 3: Runtime (`runtime`)

**Purpose:** Minimal runtime environment

**Contents:**
- Runtime dependencies only (no build tools)
- Application binaries and assets
- Service configuration
- Health check setup

**Optimizations:**
- Minimal attack surface
- Reduced image size (~200MB final)
- Fast startup time
- Resource efficiency

## 🎯 Artifact Extraction

### Automated Extraction (Recommended)

Use the dedicated extraction script:

```bash
# Extract from default image
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest

# Extract from specific image
./scripts/extract_docker_artifacts.sh hokuyo-hub:build

# Custom image with tag
./scripts/extract_docker_artifacts.sh my-registry/hokuyo-hub:v1.0.0
```

**The script will:**
- Create `dist/linux-arm64/` directory
- Extract main binary and validate ARM64 architecture
- Copy configuration files and web interface
- Generate distribution README and manifest
- Create checksums for verification
- Show detailed extraction summary

### Manual Extraction

For custom extraction requirements:

```bash
# Create temporary container
docker create --name temp-extract --platform linux/arm64 hokuyo-hub:latest

# Extract main binary
docker cp temp-extract:/opt/hokuyohub/hokuyo_hub dist/linux-arm64/

# Extract configuration
docker cp temp-extract:/opt/hokuyohub/config dist/linux-arm64/

# Extract web interface
docker cp temp-extract:/opt/hokuyohub/webui dist/linux-arm64/

# Cleanup
docker rm temp-extract
```

### Validation and Verification

```bash
# Verify binary architecture
file dist/linux-arm64/hokuyo_hub
# Expected: ELF 64-bit LSB pie executable, ARM aarch64

# Check dynamic dependencies
docker run --rm --platform linux/arm64 hokuyo-hub:latest ldd /opt/hokuyohub/hokuyo_hub

# Verify functionality
docker run --rm -p 8080:8080 --platform linux/arm64 hokuyo-hub:latest &
curl http://localhost:8080/api/health
```

## 📊 Build Performance

### Typical Build Times

| Stage | First Build | Cached Build | Incremental |
|-------|-------------|--------------|-------------|
| `build-deps` | 5-8 minutes | 30 seconds | 30 seconds |
| `build-app` | 25-35 minutes | 5-10 minutes | 2-5 minutes |
| `runtime` | 3-5 minutes | 1 minute | 1 minute |
| **Total** | **30-45 minutes** | **8-12 minutes** | **3-7 minutes** |

### Performance Optimization

**1. Enable BuildKit and Buildx:**
```bash
# Enable BuildKit for better caching
export DOCKER_BUILDKIT=1

# Use buildx for multi-platform builds
docker buildx create --name hokuyo-builder --use
```

**2. Build Cache Management:**
```bash
# Preserve build cache between builds
docker buildx build --cache-from=type=local,src=cache --cache-to=type=local,dest=cache

# Clean cache if needed
docker buildx prune
```

**3. Parallel Processing:**
```bash
# Use all available CPU cores
docker buildx build --build-arg JOBS=$(nproc)
```

**4. Registry Caching:**
```bash
# Use registry for cache (if available)
docker buildx build \
  --cache-from=type=registry,ref=myregistry/hokuyo-cache \
  --cache-to=type=registry,ref=myregistry/hokuyo-cache
```

## 🚀 Running Docker Containers

### Quick Run

```bash
# Run with default settings
docker run -d -p 8080:8080 --name hokuyo-hub hokuyo-hub:latest

# Run with custom configuration
docker run -d -p 8080:8080 \
    -v $(pwd)/configs:/opt/hokuyohub/config:ro \
    --name hokuyo-hub hokuyo-hub:latest
```

### Production Deployment

**1. With Persistent Storage:**
```bash
# Create data volume
docker volume create hokuyo-data

# Run with persistent storage
docker run -d \
    --name hokuyo-hub \
    -p 8080:8080 \
    -p 8081:8081 \
    -v hokuyo-data:/var/lib/hokuyo-hub \
    -v $(pwd)/configs:/opt/hokuyohub/config:ro \
    --restart unless-stopped \
    hokuyo-hub:latest
```

**2. With Environment Variables:**
```bash
docker run -d \
    --name hokuyo-hub \
    -p 8080:8080 \
    -e HOKUYO_CONFIG_PATH=/opt/hokuyohub/config \
    -e HOKUYO_LOG_LEVEL=INFO \
    --restart unless-stopped \
    hokuyo-hub:latest
```

**3. Health Monitoring:**
```bash
# Check container health
docker ps
docker logs hokuyo-hub

# Monitor resource usage
docker stats hokuyo-hub

# Health check endpoint
curl http://localhost:8080/api/health
```

### Docker Compose Deployment

Use the provided Docker Compose configuration for full-featured deployment:

```yaml
# docker-compose.yml
version: '3.8'
services:
  hokuyo-hub:
    build:
      context: .
      dockerfile: docker/Dockerfile.rpi
      target: runtime
      platforms:
        - linux/arm64
    ports:
      - "8080:8080"
      - "8081:8081"
    volumes:
      - ./configs:/opt/hokuyohub/config:ro
      - hokuyo-data:/var/lib/hokuyo-hub
    environment:
      - HOKUYO_CONFIG_PATH=/opt/hokuyohub/config
      - HOKUYO_LOG_LEVEL=INFO
    restart: unless-stopped
    healthcheck:
      test: ["CMD", "curl", "-f", "http://localhost:8080/api/health"]
      interval: 30s
      timeout: 10s
      retries: 3

volumes:
  hokuyo-data:
    driver: local
```

**Deploy with Compose:**
```bash
# Start services
docker-compose up -d

# View logs
docker-compose logs -f

# Scale services (if needed)
docker-compose up -d --scale hokuyo-hub=2

# Stop services
docker-compose down
```

## 🔧 Advanced Docker Configuration

### Custom Dockerfiles

**Dockerfile Variants:**
- `docker/Dockerfile.rpi` - Standard multi-stage build
- `docker/Dockerfile.rpi.fixed` - Fixed dependency versions
- `docker/Dockerfile.rpi.rebuild` - URG library rebuild version

**Using Alternative Dockerfile:**
```bash
# Use fixed version dockerfile
DOCKERFILE=docker/Dockerfile.rpi.fixed ./docker/build.sh build-all

# Or with direct buildx command
docker buildx build --file docker/Dockerfile.rpi.rebuild
```

### Registry and Distribution

**1. Push to Registry:**
```bash
# Tag for registry
docker tag hokuyo-hub:latest myregistry.com/hokuyo-hub:latest

# Push to registry
docker push myregistry.com/hokuyo-hub:latest

# Build and push in one command
docker buildx build \
    --platform linux/arm64 \
    --tag myregistry.com/hokuyo-hub:latest \
    --push \
    .
```

**2. Multi-Architecture Builds:**
```bash
# Build for multiple platforms
docker buildx build \
    --platform linux/arm64,linux/amd64 \
    --tag myregistry.com/hokuyo-hub:multi \
    --push \
    .
```

### Security Hardening

**1. Non-Root User:**
```dockerfile
# In Dockerfile
RUN groupadd -r hokuyo && useradd -r -g hokuyo hokuyo
USER hokuyo
```

**2. Security Scanning:**
```bash
# Scan for vulnerabilities
docker scout quickview hokuyo-hub:latest
docker run --rm -v /var/run/docker.sock:/var/run/docker.sock \
    -v $(pwd):/tmp aquasec/trivy image hokuyo-hub:latest
```

**3. Minimal Runtime:**
```dockerfile
# Use distroless or minimal base for runtime
FROM gcr.io/distroless/cc-debian12 AS runtime
```

## 🔍 Troubleshooting

### Build Issues

**1. Platform Mismatch:**
```bash
# Error: platform mismatch
# Solution: Specify platform explicitly
docker buildx build --platform linux/arm64 ...

# Check builder capabilities
docker buildx inspect
```

**2. Out of Memory During Build:**
```bash
# Error: build killed (OOM)
# Solution: Increase Docker memory limit
# Docker Desktop: Settings > Resources > Memory

# Or build with reduced parallelism
docker buildx build --build-arg JOBS=2
```

**3. Cache Issues:**
```bash
# Error: cache corruption
# Solution: Clear cache and rebuild
docker buildx prune -af
./docker/build.sh clean
./docker/build.sh build-all
```

### Runtime Issues

**1. Container Won't Start:**
```bash
# Check container logs
docker logs hokuyo-hub

# Run interactively for debugging
docker run -it --rm hokuyo-hub:latest /bin/bash

# Check if binary is executable
docker run --rm hokuyo-hub:latest ls -la /opt/hokuyohub/hokuyo_hub
```

**2. Architecture Mismatch:**
```bash
# Error: exec format error
# Check if running on compatible platform
docker run --rm --platform linux/arm64 hokuyo-hub:latest uname -m
# Should show: aarch64

# On non-ARM systems, emulation may be required
docker run --privileged --rm tonistiigi/binfmt --install arm64
```

**3. Network Issues:**
```bash
# Port not accessible
docker ps  # Check port mapping
docker port hokuyo-hub  # Show port mappings

# Container connectivity
docker exec hokuyo-hub ping 8.8.8.8
docker exec hokuyo-hub netstat -ln
```

### Performance Issues

**1. Slow Build Times:**
```bash
# Enable parallel builds
docker buildx build --build-arg JOBS=$(nproc)

# Use cache layers
docker buildx build --cache-from=type=local,src=cache
```

**2. Large Image Size:**
```bash
# Analyze image layers
docker history hokuyo-hub:latest

# Use dive for detailed analysis
dive hokuyo-hub:latest

# Multi-stage builds help minimize runtime size
```

**3. Runtime Performance:**
```bash
# Monitor resource usage
docker stats hokuyo-hub

# Adjust resource limits
docker run -d --memory=1g --cpus=2 hokuyo-hub:latest

# Use performance governor in container
docker run --privileged hokuyo-hub:latest
```

## 🧪 Testing and Validation

### Build Testing

```bash
# Test build process without creating runtime
./docker/build.sh test-build

# Validate build artifacts
./docker/build.sh build-app
docker run --rm --platform linux/arm64 hokuyo-hub:build file /build/build/linux-arm64/hokuyo_hub
```

### Runtime Testing

```bash
# Test container startup
./docker/build.sh test-run

# Manual testing
docker run -d -p 8080:8080 --name test-hokuyo hokuyo-hub:latest
sleep 10
curl http://localhost:8080/api/health
docker stop test-hokuyo && docker rm test-hokuyo
```

### Integration Testing

**Automated Test Script:**
```bash
#!/bin/bash
# test-docker-build.sh

set -e

echo "Building Docker image..."
./docker/build.sh build-all

echo "Extracting artifacts..."
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest

echo "Testing extracted binary..."
file dist/linux-arm64/hokuyo_hub

echo "Starting container..."
docker run -d -p 8080:8080 --name hokuyo-test hokuyo-hub:latest

sleep 15

echo "Testing API endpoints..."
curl -f http://localhost:8080/api/health
curl -f http://localhost:8080/api/sensors

echo "Cleanup..."
docker stop hokuyo-test && docker rm hokuyo-test

echo "All tests passed!"
```

## 📚 Additional Resources

- **[Main Build Guide](../../BUILD.md)** - Overview of all build methods
- **[Quick Start Guide](../../QUICK_START.md)** - Get running quickly
- **[Raspberry Pi Guide](raspberry-pi.md)** - Target platform setup
- **[Troubleshooting Guide](troubleshooting.md)** - Common issues and solutions
- **[Advanced Configuration](advanced.md)** - Expert-level customization

### Docker Resources

- **[Docker Multi-Platform Builds](https://docs.docker.com/buildx/working-with-buildx/)**
- **[Docker Compose Reference](https://docs.docker.com/compose/compose-file/)**
- **[Dockerfile Best Practices](https://docs.docker.com/develop/dev-best-practices/)**

---

**Docker Build Guide Version:** 1.0  
**Last Updated:** 2025-08-27  
**Target Platform:** linux/arm64 (Raspberry Pi 5)  
**Build System:** Docker Multi-stage with Debian Bookworm  
**Output Structure:** `dist/linux-arm64/` (platform-specific)