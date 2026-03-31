# CLAUDE.md — HokuyoHub

## Project Overview

HokuyoHub is a real-time LiDAR sensor data processing and visualization platform for Hokuyo sensors. C++20 backend with CrowCpp web framework, JavaScript ES6+ frontend. Collects, processes (DBSCAN clustering), and publishes data at 30 FPS via NNG/OSC/REST.

## Architecture

```
Web Frontend (JS ES6+, Canvas API)
  ↓ HTTP/WebSocket (CrowCpp)
Application Logic (SensorManager, FilterManager, PublisherManager)
  ↓
Data Processing (DBSCAN2D, PreFilter, PostFilter, Transform)
  ↓
Hardware Layer (URG Library → Hokuyo sensors)
```

## Source Structure

```
src/
├── main.cpp                        # Entry point, signal handling, Crow setup
├── config/config.h|cpp             # YAML config parsing & structures
├── core/
│   ├── sensor_manager.h|cpp        # Sensor lifecycle & data frame generation
│   ├── filter_manager.h|cpp        # Filter pipeline coordination
│   ├── mask.h|cpp                  # ROI masking
│   └── transform.h                 # Coordinate transformations
├── detect/
│   ├── dbscan.h|cpp                # DBSCAN2D clustering
│   ├── prefilter.h|cpp             # Pre-clustering filters (neighborhood, spike, outlier)
│   └── postfilter.h|cpp            # Post-clustering filters (isolation removal)
├── io/
│   ├── rest_handlers.h|cpp         # REST API endpoints
│   ├── ws_handlers.h|cpp           # WebSocket handlers
│   ├── publisher_manager.h|cpp     # Data publishing coordination
│   ├── nng_bus.h|cpp               # NNG messaging (MessagePack)
│   └── osc_publisher.h|cpp         # OSC protocol
└── sensors/
    ├── ISensor.h                   # Sensor interface
    ├── SensorFactory.h|cpp         # Factory pattern for sensor creation
    └── hokuyo/HokuyoSensorUrg.h|cpp  # URG library integration
```

Other key directories:
- `cmake/` — Dependencies.cmake, cross-compilation toolchains
- `configs/` — YAML configuration files
- `webui/` — Frontend JavaScript/HTML/CSS
- `docker/` — Docker multi-stage build for ARM64
- `scripts/` — Build, setup, testing, utility scripts
- `external/urg_library/` — Bundled URG C library

## Build System

**Language:** C++20 | **Build tool:** CMake 3.18+ | **Dependencies:** cmake/Dependencies.cmake

### Build Commands

```bash
# macOS native (development)
./scripts/build/build_with_presets.sh release --install
# Output: dist/darwin-arm64/

# Docker cross-compile for ARM64 (Raspberry Pi 5 production)
./scripts/build/docker_cross_build.sh --build-all
# Output: dist/linux-arm64/

# Manual CMake
cmake --preset mac-release
cmake --build build/darwin-arm64 --parallel
cmake --install build/darwin-arm64
```

### CMake Presets (CMakePresets.json)
- macOS: `mac-debug`, `mac-release`, `mac-relwithdebinfo`
- Raspberry Pi: `rpi-debug`, `rpi-release`, `rpi-relwithdebinfo`

### Dependencies
CrowCpp (header-only web framework), yaml-cpp, NNG, nlohmann/json (header-only), URG C library, liblo (optional, OSC). Managed via `cmake/Dependencies.cmake` with modes: `auto`, `system`, `fetch`, `bundled`.

## Running

```bash
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080
# Web UI: http://localhost:8080
```

## Testing

```bash
./scripts/testing/test_rest_api.sh http://localhost:8080
```

## Coding Conventions

- **Classes:** PascalCase (`SensorManager`, `DBSCAN2D`)
- **Functions:** camelCase (`registerRoutes`, `applyPatch`)
- **Variables:** snake_case (`sensor_id`, `frame_count`)
- **Member variables:** trailing underscore (`sensors_`, `config_`)
- **Include guards:** `#pragma once`
- **Comments:** Japanese for domain-specific details, English for general logic. Mathematical notation (Δθ, σ_r) in algorithm docs
- **Thread safety:** `std::atomic<bool>`, `std::mutex`
- **Error handling:** Early returns, logging via `std::cout` with `[Component]` tags
- **Smart pointers:** `std::shared_ptr`, `std::unique_ptr` — no raw new/delete
- **Modern C++20:** `std::span`, structured bindings, move semantics

## Key Data Structures

```cpp
struct ScanFrame {
    uint64_t t_ns;              // Timestamp (nanoseconds)
    uint32_t seq;               // Frame sequence number
    std::vector<float> xy;      // Interleaved [x0,y0,x1,y1,...]
    std::vector<uint8_t> sid;   // Sensor IDs (0-255)
};

struct Cluster {
    uint32_t id;
    uint8_t sensor_mask;
    float cx, cy, minx, miny, maxx, maxy;  // Centroid & bounding box
    std::vector<size_t> point_indices;
};
```

## REST API

- `GET /api/sensors` — List active sensors
- `GET /api/config` — Current configuration
- `GET /api/health` — Health check
- `POST /api/dbscan` — Update DBSCAN parameters

## Platform Support

- macOS (ARM64) — development
- Linux ARM64 (Raspberry Pi 5) — production deployment
- Docker cross-compilation supported
