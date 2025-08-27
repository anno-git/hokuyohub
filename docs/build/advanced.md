# Advanced Build Configuration

**Expert-level customization and optimization for HokuyoHub builds**

This guide covers advanced build configurations, performance optimizations, and expert-level customizations for HokuyoHub. For basic build instructions, see the [main BUILD.md](../../BUILD.md).

## 🌟 CrowCpp Migration Benefits

HokuyoHub now uses **CrowCpp** instead of the previous web framework, delivering significant build and performance improvements:

- **⚡ Faster Builds**: Header-only design eliminates framework compilation time
- **🪶 Simplified Dependencies**: No complex web framework dependencies to manage
- **🔧 Easier Configuration**: Reduced build configuration complexity
- **📈 Better Performance**: Lower memory footprint and faster startup times

## 🎯 Advanced CMake Configuration

### Custom Build Types

**Create Custom Build Configurations:**

```bash
# High-performance optimized build
cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -flto" \
  -DCMAKE_EXE_LINKER_FLAGS="-flto" \
  --preset mac-release

# Debug build with sanitizers
cmake \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_CXX_FLAGS="-g -O0 -fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  --preset mac-debug

# Profile-guided optimization (PGO)
cmake \
  -DCMAKE_CXX_FLAGS="-fprofile-generate" \
  --preset mac-release
# ... run profiling workload ...
cmake \
  -DCMAKE_CXX_FLAGS="-fprofile-use" \
  --preset mac-release
```

### Advanced Dependency Management

**Selective Dependency Building:**

```bash
# Build only specific dependencies from source
cmake \
  -DDEPS_MODE=auto \
  -DDEPS_YAMLCPP=system \
  -DDEPS_NNG=bundled \
  -DDEPS_URG=fetch \
  --preset mac-release

# Use specific dependency versions
cmake \
  -DYAMLCPP_VERSION=0.8.0 \
  -DNNG_VERSION=1.6.0 \
  --preset mac-release

# Note: CrowCpp is header-only and requires no build configuration
```

**Custom Dependency Locations:**

```bash
# Use custom library installations
cmake \
  -DCMAKE_PREFIX_PATH="/usr/local;/opt/custom" \
  -DyamlCpp_ROOT=/opt/custom/yaml-cpp \
  --preset mac-release

# CrowCpp is included as header-only - no custom location needed
```

### Feature Configuration Matrix

**Complete Feature Toggle Reference:**

| Option | Default | Description |
|--------|---------|-------------|
| `HOKUYO_NNG_ENABLE` | `ON` | Enable NNG publisher support |
| `USE_OSC` | `ON` | Enable OSC protocol support |
| `BUILD_SHARED_LIBS` | `OFF` | Build shared libraries |
| `CMAKE_INTERPROCEDURAL_OPTIMIZATION` | `OFF` | Enable LTO |
| `BUILD_TESTING` | `OFF` | Build unit tests |
| `BUILD_EXAMPLES` | `OFF` | Build example applications |
| `ENABLE_CLANG_TIDY` | `OFF` | Enable clang-tidy checks |
| `ENABLE_CPPCHECK` | `OFF` | Enable cppcheck analysis |
| `USE_CCACHE` | `OFF` | Use compiler cache |

**Example Feature Combinations:**

```bash
# Development build with all tools
cmake \
  -DBUILD_TESTING=ON \
  -DBUILD_EXAMPLES=ON \
  -DENABLE_CLANG_TIDY=ON \
  -DUSE_CCACHE=ON \
  --preset mac-debug

# Minimal production build
cmake \
  -DHOKUYO_NNG_ENABLE=OFF \
  -DUSE_OSC=OFF \
  -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
  --preset mac-release

# Static linking build
cmake \
  -DBUILD_SHARED_LIBS=OFF \
  -DCMAKE_EXE_LINKER_FLAGS="-static-libgcc -static-libstdc++" \
  --preset mac-release
```

## 🔧 Custom Toolchain Configuration

### Advanced Cross-Compilation

**Custom Toolchain File:**

```cmake
# custom-toolchain.cmake
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)

# Custom toolchain prefix
set(CROSS_COMPILE_PREFIX "my-custom-aarch64-")
set(CMAKE_C_COMPILER "${CROSS_COMPILE_PREFIX}gcc")
set(CMAKE_CXX_COMPILER "${CROSS_COMPILE_PREFIX}g++")
set(CMAKE_AR "${CROSS_COMPILE_PREFIX}ar")

# Custom sysroot
set(CMAKE_SYSROOT "/path/to/custom/sysroot")
set(CMAKE_FIND_ROOT_PATH "${CMAKE_SYSROOT}")

# Custom compiler flags
set(CMAKE_C_FLAGS_INIT "-mcpu=cortex-a76 -O3")
set(CMAKE_CXX_FLAGS_INIT "-mcpu=cortex-a76 -O3")
```

**Usage:**
```bash
cmake -DCMAKE_TOOLCHAIN_FILE=custom-toolchain.cmake .
```

### Compiler-Specific Optimizations

**GCC Advanced Optimizations:**

```bash
cmake \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -mtune=native -flto=auto -fuse-linker-plugin" \
  -DCMAKE_EXE_LINKER_FLAGS="-flto=auto -fuse-linker-plugin" \
  --preset mac-release
```

**Clang Advanced Optimizations:**

```bash
cmake \
  -DCMAKE_CXX_FLAGS="-O3 -march=native -flto=thin" \
  -DCMAKE_EXE_LINKER_FLAGS="-flto=thin" \
  --preset mac-release
```

**Platform-Specific ARM64 Tuning:**

```bash
# Raspberry Pi 5 specific
cmake \
  -DCMAKE_CXX_FLAGS="-mcpu=cortex-a76 -mtune=cortex-a76 -mfpu=neon-fp-armv8" \
  --preset rpi-release

# Apple Silicon specific
cmake \
  -DCMAKE_CXX_FLAGS="-mcpu=apple-m1 -mtune=apple-m1" \
  --preset mac-release
```

## 🐳 Advanced Docker Configuration

### Multi-Stage Optimization

**Custom Multi-Stage Dockerfile:**

```dockerfile
# Dockerfile.advanced
# Build-time optimization stage
FROM debian:bookworm-slim AS optimizer
RUN apt-get update && apt-get install -y upx-ucl strip
COPY --from=builder /build/hokuyo_hub /tmp/hokuyo_hub
RUN strip /tmp/hokuyo_hub && upx --best /tmp/hokuyo_hub

# Minimal runtime with optimization
FROM gcr.io/distroless/cc-debian12 AS runtime
COPY --from=optimizer /tmp/hokuyo_hub /opt/hokuyo-hub/hokuyo_hub
```

**Build Cache Optimization:**

```bash
# Advanced cache configuration
docker buildx build \
  --platform linux/arm64 \
  --cache-from=type=registry,ref=myregistry/cache:buildcache \
  --cache-to=type=registry,ref=myregistry/cache:buildcache,mode=max \
  --cache-from=type=local,src=.buildx-cache \
  --cache-to=type=local,dest=.buildx-cache-new,mode=max \
  .

# Rotate local cache
rm -rf .buildx-cache
mv .buildx-cache-new .buildx-cache
```

### Security Hardening

**Secure Container Build:**

```dockerfile
# Security-hardened runtime
FROM debian:bookworm-slim AS runtime

# Create non-root user
RUN groupadd -r -g 1000 hokuyo && \
    useradd -r -u 1000 -g hokuyo -s /bin/false -M hokuyo

# Install minimal runtime dependencies
RUN apt-get update && apt-get install -y --no-install-recommends \
    libyaml-cpp0.7 libnng1 libssl3 zlib1g ca-certificates && \
    apt-get clean && rm -rf /var/lib/apt/lists/*

# Set up secure filesystem
RUN mkdir -p /opt/hokuyo-hub /var/lib/hokuyo-hub && \
    chown -R hokuyo:hokuyo /opt/hokuyo-hub /var/lib/hokuyo-hub && \
    chmod 755 /opt/hokuyo-hub && chmod 750 /var/lib/hokuyo-hub

# Switch to non-root user
USER hokuyo:hokuyo

# Security labels
LABEL security.scan="enabled"
LABEL security.hardening="minimal-runtime,non-root-user"
```

## 📊 Performance Tuning

### Compile-Time Optimizations

**Template and Compilation Optimizations:**

```bash
# Faster compilation
cmake \
  -DCMAKE_CXX_FLAGS="-pipe -fno-plt -ffast-math" \
  -DCMAKE_SHARED_LINKER_FLAGS="-Wl,-O1,--sort-common,--as-needed,-z,relro,-z,now" \
  --preset mac-release

# Reduce binary size
cmake \
  -DCMAKE_CXX_FLAGS="-Os -ffunction-sections -fdata-sections" \
  -DCMAKE_EXE_LINKER_FLAGS="-Wl,--gc-sections" \
  --preset mac-release
```

**Memory Layout Optimization:**

```bash
# Optimize memory layout
cmake \
  -DCMAKE_CXX_FLAGS="-fprofile-arcs -ftest-coverage" \
  --preset mac-release
# Run profiling workload
cmake \
  -DCMAKE_CXX_FLAGS="-fprofile-use -fprofile-correction" \
  --preset mac-release
```

### Runtime Performance Tuning

**System-Level Optimizations:**

```bash
# CPU governor (Linux/Raspberry Pi)
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Memory management
echo 1 | sudo tee /proc/sys/vm/swappiness
echo never | sudo tee /sys/kernel/mm/transparent_hugepage/enabled

# Network stack optimization
echo 'net.core.rmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
echo 'net.core.wmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

**Application-Level Tuning:**

```yaml
# configs/performance.yaml
sensors:
  - id: "high_performance_sensor"
    skip_step: 1          # No downsampling
    interval: 0           # Maximum scan rate
    buffer_size: 8192     # Larger buffer

dbscan:
  eps_norm: 2.0          # Balanced accuracy/performance
  minPts: 3              # Faster clustering
  h_min: 0.01           # Fine resolution
  h_max: 0.15           # Reasonable max
  M_max: 1000           # More candidates for accuracy

processing:
  thread_count: 4        # Use multiple cores
  priority: high         # Higher process priority
  affinity: [0, 1, 2, 3] # Pin to specific cores
```

## 🔬 Development Tools Integration

### Static Analysis Tools

**Clang-Tidy Configuration:**

```yaml
# .clang-tidy
Checks: >
  bugprone-*,
  cert-*,
  clang-analyzer-*,
  cppcoreguidelines-*,
  modernize-*,
  performance-*,
  readability-*
WarningsAsErrors: >
  bugprone-use-after-move,
  cert-dcl21-cpp,
  clang-analyzer-security.*
```

```bash
# Build with clang-tidy
cmake -DENABLE_CLANG_TIDY=ON --preset mac-debug
```

**Address Sanitizer Integration:**

```bash
# Build with AddressSanitizer
cmake \
  -DCMAKE_CXX_FLAGS="-fsanitize=address,undefined -fno-omit-frame-pointer" \
  -DCMAKE_EXE_LINKER_FLAGS="-fsanitize=address,undefined" \
  --preset mac-debug

# Run with ASAN options
export ASAN_OPTIONS=verbosity=1:halt_on_error=1:check_initialization_order=1
./hokuyo_hub --config configs/default.yaml
```

**Valgrind Integration:**

```bash
# Build debug version
cmake --preset mac-debug
cmake --build build/darwin-arm64

# Run with Valgrind
valgrind \
  --tool=memcheck \
  --leak-check=full \
  --track-origins=yes \
  --log-file=valgrind.log \
  ./dist/darwin-arm64/hokuyo_hub --config ./configs/default.yaml
```

### Profiling and Benchmarking

**CPU Profiling with Instruments (macOS):**

```bash
# Profile application
instruments -t "Time Profiler" -D profile.trace ./hokuyo_hub &
# ... let it run for profiling period ...
kill %1

# View results
open profile.trace
```

**Memory Profiling:**

```bash
# Heap profiling with jemalloc
export MALLOC_CONF="prof:true,prof_prefix:jeprof.out"
./hokuyo_hub --config configs/default.yaml

# Generate heap profile
jeprof --pdf ./hokuyo_hub jeprof.out.* > heap_profile.pdf
```

**Performance Benchmarking:**

```cpp
// benchmark/sensor_performance.cpp
#include <benchmark/benchmark.h>
#include "sensor_manager.h"

static void BenchmarkSensorProcessing(benchmark::State& state) {
    SensorManager manager;
    for (auto _ : state) {
        manager.processScanData(test_data);
    }
    state.SetItemsProcessed(state.iterations());
}

BENCHMARK(BenchmarkSensorProcessing);
BENCHMARK_MAIN();
```

```bash
# Build benchmarks
cmake -DBUILD_BENCHMARKS=ON --preset mac-release
cmake --build build/darwin-arm64 --target benchmarks

# Run benchmarks
./build/darwin-arm64/benchmark/sensor_performance
```

## 🌐 CI/CD Integration

### GitHub Actions Advanced Configuration

```yaml
# .github/workflows/advanced-build.yml
name: Advanced Build Matrix

on: [push, pull_request]

jobs:
  build-matrix:
    strategy:
      matrix:
        os: [macos-12, macos-13, ubuntu-20.04, ubuntu-22.04]
        compiler: [gcc, clang]
        build_type: [Debug, Release, RelWithDebInfo]
        deps_mode: [auto, system, fetch]
        exclude:
          - os: macos-12
            compiler: gcc
          - os: macos-13
            compiler: gcc
        include:
          - os: macos-13
            compiler: clang
            build_type: Release
            deps_mode: system
            extra_flags: "-DCMAKE_CXX_FLAGS='-march=native -flto'"

    runs-on: ${{ matrix.os }}
    
    steps:
    - uses: actions/checkout@v4
      with:
        submodules: recursive
        
    - name: Setup Build Environment
      run: |
        if [[ "${{ matrix.os }}" == "macos-"* ]]; then
          brew install cmake yaml-cpp
        else
          sudo apt update
          sudo apt install -y cmake libyaml-cpp-dev
        fi
        
    - name: Configure Build
      run: |
        cmake \
          -DCMAKE_BUILD_TYPE=${{ matrix.build_type }} \
          -DDEPS_MODE=${{ matrix.deps_mode }} \
          -DCMAKE_CXX_COMPILER=${{ matrix.compiler == 'clang' && 'clang++' || 'g++' }} \
          ${{ matrix.extra_flags }} \
          --preset ${{ matrix.os == 'macos-12' && 'mac-release' || 'mac-release' }}
          
    - name: Build
      run: cmake --build build/darwin-arm64 --parallel
      
    - name: Test
      run: ctest --test-dir build/darwin-arm64 --verbose
      
    - name: Package
      if: matrix.build_type == 'Release'
      run: |
        cmake --install build/darwin-arm64
        tar -czf hokuyo-hub-${{ matrix.os }}-${{ matrix.compiler }}.tar.gz dist/

    - name: Upload Artifacts
      uses: actions/upload-artifact@v4
      if: matrix.build_type == 'Release'
      with:
        name: hokuyo-hub-${{ matrix.os }}-${{ matrix.compiler }}
        path: hokuyo-hub-*.tar.gz
```

### Docker Multi-Architecture CI

```yaml
# Docker multi-arch build
docker-multiarch:
  runs-on: ubuntu-latest
  steps:
    - name: Set up QEMU
      uses: docker/setup-qemu-action@v3
      
    - name: Set up Docker Buildx
      uses: docker/setup-buildx-action@v3
      
    - name: Build multi-arch
      uses: docker/build-push-action@v5
      with:
        platforms: linux/amd64,linux/arm64
        tags: hokuyo-hub:multi
        cache-from: type=gha
        cache-to: type=gha,mode=max
```

## 🔐 Security Hardening

### Secure Build Practices

**Compiler Security Features:**

```bash
# Security-hardened build
cmake \
  -DCMAKE_CXX_FLAGS="-fstack-protector-strong -D_FORTIFY_SOURCE=2 -fPIE" \
  -DCMAKE_EXE_LINKER_FLAGS="-pie -Wl,-z,relro,-z,now" \
  --preset mac-release
```

**Static Analysis Security Checks:**

```bash
# Security-focused static analysis
cmake -DENABLE_CLANG_TIDY=ON -DCLANG_TIDY_CHECKS="cert-*,security-*" --preset mac-debug
```

### Supply Chain Security

**Dependency Verification:**

```bash
# Verify dependency checksums
cmake -DVERIFY_DEPENDENCY_CHECKSUMS=ON --preset mac-release

# Use signed dependencies only
cmake -DREQUIRE_SIGNED_DEPENDENCIES=ON --preset mac-release
```

**Build Reproducibility:**

```bash
# Reproducible builds
export SOURCE_DATE_EPOCH=1234567890
cmake \
  -DCMAKE_CXX_FLAGS="-fdebug-prefix-map=$PWD=." \
  -DCMAKE_BUILD_TIMESTAMP=1234567890 \
  --preset mac-release
```

## 🧬 Custom Extensions

### Plugin Architecture

**Custom Sensor Plugin:**

```cpp
// plugins/custom_sensor.cpp
#include "sensor_plugin_interface.h"

class CustomSensorPlugin : public SensorPlugin {
public:
    bool initialize(const YAML::Node& config) override {
        // Initialize custom sensor
        return true;
    }
    
    ScanData getScanData() override {
        // Return custom scan data
        return custom_scan_data;
    }
};

extern "C" SensorPlugin* create_plugin() {
    return new CustomSensorPlugin();
}
```

**Plugin Build Integration:**

```cmake
# CMakeLists.txt plugin support
add_library(custom_sensor_plugin SHARED plugins/custom_sensor.cpp)
target_link_libraries(custom_sensor_plugin PRIVATE hokuyo_core)
set_target_properties(custom_sensor_plugin PROPERTIES PREFIX "")
```

### Custom Processing Modules

**DBSCAN Algorithm Customization:**

```cpp
// processing/custom_dbscan.h
template<typename PointType, typename DistanceFunc>
class CustomDBSCAN {
private:
    DistanceFunc distance_function;
    
public:
    CustomDBSCAN(DistanceFunc func) : distance_function(func) {}
    
    std::vector<Cluster> cluster(const std::vector<PointType>& points,
                                double eps, size_t min_pts);
};
```

## 📈 Monitoring and Telemetry

### Performance Monitoring Integration

**Prometheus Metrics:**

```cpp
// monitoring/prometheus_exporter.cpp
#include <prometheus/counter.h>
#include <prometheus/histogram.h>

class MetricsExporter {
    prometheus::Counter& scan_counter;
    prometheus::Histogram& processing_duration;
    
public:
    void recordScan() { scan_counter.Increment(); }
    void recordProcessingTime(double duration) {
        processing_duration.Observe(duration);
    }
};
```

**Custom Telemetry:**

```yaml
# configs/telemetry.yaml
telemetry:
  enabled: true
  endpoint: "http://telemetry.example.com"
  metrics:
    - sensor_fps
    - processing_latency
    - cluster_count
  export_interval: 10  # seconds
```

## 🎛️ Advanced Configuration Templates

### High-Performance Template

```yaml
# configs/high-performance.yaml
sensors:
  - id: "perf_sensor"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"
    skip_step: 1
    interval: 0
    buffer_size: 16384

dbscan:
  eps_norm: 2.0
  minPts: 3
  h_min: 0.005
  h_max: 0.20
  R_max: 10
  M_max: 2000

processing:
  threads: 8
  priority: realtime
  cpu_affinity: [0, 1, 2, 3]

performance:
  memory_pool_size: 1048576
  zero_copy_buffers: true
  lock_free_queues: true
```

### Low-Latency Template

```yaml
# configs/low-latency.yaml
sensors:
  - id: "latency_sensor"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"
    skip_step: 2
    interval: 25
    immediate_processing: true

dbscan:
  eps_norm: 3.0
  minPts: 5
  adaptive_resolution: true

filtering:
  prefilter:
    enabled: false  # Disable for lower latency
  postfilter:
    enabled: false

sinks:
  - type: "nng"
    url: "ipc:///tmp/hokuyo-fast"
    zero_copy: true
    immediate_send: true
```

### Resource-Constrained Template

```yaml
# configs/resource-constrained.yaml
sensors:
  - id: "constrained_sensor"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"
    skip_step: 4
    interval: 200
    buffer_size: 2048

dbscan:
  eps_norm: 4.0
  minPts: 8
  h_min: 0.05
  h_max: 0.10
  M_max: 200

memory:
  limit_mb: 64
  gc_interval: 5000
  small_object_pool: true

processing:
  threads: 2
  batch_size: 100
```

---

**Advanced Configuration Guide Version:** 1.0  
**Last Updated:** 2025-08-27  
**Covers:** Expert CMake, Performance Tuning, Security, Extensions  
**Target Audience:** Advanced developers, DevOps engineers, System architects