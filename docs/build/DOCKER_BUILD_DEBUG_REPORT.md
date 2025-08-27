# Docker Build System Debugging Report

## Summary

This document details the systematic debugging process and solutions implemented for the Hokuyo Hub Docker ARM64 cross-compilation build system.

## Problem Description

During Phase 2 script reorganization, multiple critical issues were discovered in the Docker build system that prevented successful ARM64 cross-compilation builds for Raspberry Pi 5.

## Issues Identified and Solutions

### 1. GCC Compiler Strictness Issue

**Problem**: `-Werror=restrict` flag causing build failures in Drogon framework examples.

**Error**: 
```
warning: 'void* __builtin_memcpy(void*, const void*, long unsigned int)' accessing 9223372036854775810 or more bytes at offsets [2, 9223372036854775807] and 1 may overlap up to 9223372036854775813 bytes at offset -3 [-Wrestrict]
```

**Solution**: Added compiler flags to disable the restrict error:
```cmake
-DCMAKE_CXX_FLAGS="-Wno-error=restrict"
-DCMAKE_C_FLAGS="-Wno-error=restrict"
```

**Location**: `docker/Dockerfile.rpi` line 102

### 2. URG Library Architecture Mismatch

**Problem**: The bundled URG library (`third_party/urg_library/lib/liburg_c.a`) was compiled for macOS ARM64 (Mach-O format) but needed for Linux ARM64 (ELF format).

**Error**: Linking failures during hokuyo_hub binary creation.

**Solution**: Added build step to rebuild URG library from source during Docker build:
```dockerfile
RUN echo "=== Rebuilding URG Library for Linux ARM64 ===" && \
    cd external/urg_library/current && \
    make clean && \
    make && \
    cp -r include/* /build/third_party/urg_library/include/ && \
    cp src/liburg_c.a /build/third_party/urg_library/lib/ && \
    echo "URG library rebuilt successfully"
```

**Location**: `docker/Dockerfile.rpi` lines 93-100

### 3. Missing Build Tools

**Problem**: `file` command not available for binary verification step.

**Solution**: Added `file` package to build dependencies:
```dockerfile
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    ninja-build \
    pkg-config \
    git \
    file \  # Added this line
    libyaml-cpp-dev \
    libnng-dev \
    libssl-dev \
    zlib1g-dev \
    libjsoncpp-dev \
    libbrotli-dev \
    libuuid1 \
    uuid-dev \
    && rm -rf /var/lib/apt/lists/*
```

**Location**: `docker/Dockerfile.rpi` lines 43-57

### 4. Path Inconsistency Issues

**Problem**: Inconsistency between CMAKE_INSTALL_PREFIX and Docker COPY paths.

**Issues Found**:
- CMake install prefix: `/opt/hokuyohub` (without dash)
- Docker COPY paths: `/opt/hokuyo-hub` (with dash)

**Solution**: Updated all paths to use consistent naming:
```dockerfile
# Before
COPY --from=build-app /staging/opt/hokuyo-hub /opt/hokuyo-hub

# After  
COPY --from=build-app /staging/opt/hokuyohub /opt/hokuyohub
```

**Locations**: `docker/Dockerfile.rpi` lines 144, 147, 157

### 5. Artifact Extraction Path Issues

**Problem**: Extraction script looking in wrong container layers and paths.

**Solution**: Updated extraction script to prioritize runtime container paths:
```bash
# Primary path: runtime container installed location
if docker cp "$TEMP_CONTAINER:/opt/hokuyohub/hokuyo_hub" "$DIST_DIR/" 2>/dev/null; then
    log_success "Main binary extracted successfully"
# Fallback paths for different build contexts
else
    # ... fallback logic
fi
```

**Location**: `scripts/utils/extract_docker_artifacts.sh` lines 78-95

## Technical Architecture

### Multi-Stage Docker Build

The build system uses a 3-stage Docker approach:

1. **Build Dependencies (`build-deps`)**: Base Debian Bookworm with all compilation tools
2. **Application Build (`build-app`)**: Builds the application with rebuilt URG library
3. **Runtime (`runtime`)**: Minimal runtime container with only necessary libraries

### Key Build Features

- **URG Library Cross-Compilation**: Rebuilds URG library from source for correct architecture
- **Static Drogon Linking**: Uses Drogon fetch mode for optimal static linking
- **System Library Integration**: Uses system libraries for yaml-cpp and nng for efficiency
- **Comprehensive Verification**: Includes binary verification with `file` and `ldd` commands

## Performance Results

- **Binary Size**: 2.98 MB (ARM64 Linux executable)
- **Distribution Size**: 3.2 MB total including config and WebUI
- **Container Size**: 208 MB runtime (significant reduction from build container)
- **Build Time**: ~5-10 minutes depending on cache hits

## Verification

The final Docker build produces:

```
build/linux-arm64/hokuyo_hub: ELF 64-bit LSB pie executable, ARM aarch64, version 1 (GNU/Linux), dynamically linked, interpreter /lib/ld-linux-aarch64.so.1, BuildID[sha1]=3910bac736a6f4e417fdbf4ef6a605187491f42d, for GNU/Linux 3.7.0, with debug_info, not stripped
```

**Dependencies Verified**:
- libyaml-cpp.so.0.7
- libssl.so.3, libcrypto.so.3  
- libjsoncpp.so.25
- libuuid.so.1
- libbrotli1, libz.so.1
- libnng.so.1
- Standard system libraries (libc, libstdc++, etc.)

## Script Integration

The Docker build system integrates with the reorganized script structure:

- **`scripts/build/docker_cross_build.sh`**: Main wrapper script
- **`docker/build.sh`**: Core Docker build orchestration
- **`scripts/utils/extract_docker_artifacts.sh`**: Artifact extraction for deployment

## Deployment Ready

The final distribution includes:

- `hokuyo_hub` - ARM64 executable (2.98 MB)
- `config/default.yaml` - Configuration file
- `webui/` - Complete web interface (17 files)
- `README.md` - Deployment instructions
- `MANIFEST.txt` - File inventory
- `CHECKSUMS.txt` - SHA256 checksums for verification

## Recommendations

1. **Container Registry**: Consider pushing to a container registry for easier deployment
2. **Health Checks**: The runtime container includes health check endpoints
3. **Security**: Runtime container runs as non-root user `hokuyo`
4. **Monitoring**: Consider adding application metrics for production monitoring

## Conclusion

The Docker build system now provides a fully functional ARM64 cross-compilation environment that produces deployment-ready binaries for Raspberry Pi 5 systems. All critical issues have been resolved through systematic debugging and testing.