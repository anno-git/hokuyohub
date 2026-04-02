# CLAUDE.md — HokuyoHub

## Project Overview

HokuyoHub is a real-time LiDAR sensor data processing and visualization platform for Hokuyo sensors. C++20 backend with CrowCpp web framework, JavaScript ES6+ frontend with Node.js proxy server. Collects, processes (DBSCAN clustering), and publishes data via NNG/OSC/REST/WebSocket.

## Architecture

```
Web Frontend (JS ES6+, Canvas API)
  ↓ HTTP proxy (Express/Node.js)
Backend (CrowCpp HTTP/WebSocket)
  ↓
Application Logic (SensorManager, FilterManager, PublisherManager)
  ↓
Data Processing (DBSCAN2D, PreFilter, PostFilter, Transform, WorldMask)
  ↓
Hardware Layer (URG Library → Hokuyo sensors)
```

### Data Pipeline

```
Sensor callback (ScanFrame)
  → WebSocket: raw points (pushRawLite)
  → PreFilter (neighborhood, spike, outlier, intensity, isolation)
  → World mask (polygon include/exclude)
  → WebSocket: filtered points (pushFilteredLite)
  → DBSCAN2D clustering
  → PostFilter (isolation removal)
  → WebSocket: clusters (pushClustersLite)
  → PublisherManager: publishClusters() [NNG/OSC, if send_clusters=true]
  → PublisherManager: publishRaw()      [NNG/OSC, if send_raw=true]
```

## Source Structure

```
src/
├── main.cpp                        # Entry point, signal handling, Crow setup
├── config/config.h|cpp             # YAML config parsing & structures
├── core/
│   ├── sensor_manager.h|cpp        # Sensor lifecycle & data frame generation
│   ├── filter_manager.h|cpp        # Filter pipeline coordination
│   ├── mask.h|cpp                  # ROI masking (polygon include/exclude)
│   └── transform.h                 # Coordinate transformations
├── detect/
│   ├── dbscan.h|cpp                # DBSCAN2D clustering
│   ├── prefilter.h|cpp             # Pre-clustering filters (neighborhood, spike, outlier, intensity, isolation)
│   └── postfilter.h|cpp            # Post-clustering filters (isolation removal)
├── io/
│   ├── rest_handlers.h|cpp         # REST API endpoints (v1)
│   ├── ws_handlers.h|cpp           # WebSocket handlers
│   ├── publisher_manager.h|cpp     # Data publishing coordination
│   ├── nng_bus.h|cpp               # NNG pub/sub (MessagePack/JSON)
│   └── osc_publisher.h|cpp         # OSC protocol (UDP)
└── sensors/
    ├── ISensor.h                   # Sensor interface
    ├── SensorFactory.h|cpp         # Factory pattern for sensor creation
    └── hokuyo/HokuyoSensorUrg.h|cpp  # URG library integration
```

Other key directories:
- `cmake/` — Dependencies.cmake, cross-compilation toolchains
- `configs/` — YAML configuration files (`default.yaml`, `production.yaml`)
- `webui-server/` — Node.js Express proxy server + frontend JavaScript/HTML/CSS
- `docker/` — Docker multi-stage build for ARM64
- `scripts/` — Build, dev, install, ops, testing, packaging scripts
- `docs/` — Build, deployment, development documentation
- `tests/` — Integration, performance, QA test suites
- `external/urg_library/` — Bundled URG C library
- `external/yaml-cpp/` — Bundled yaml-cpp library

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
# Backend only
./dist/darwin-arm64/hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8081

# Frontend dev server (proxies to backend)
cd webui-server && npm start
# Web UI: http://localhost:3000
```

## Testing

```bash
./scripts/testing/test_rest_api.sh http://localhost:8081
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
    std::vector<float> xy;      // Interleaved [x0,y0,x1,y1,...] in meters
    std::vector<uint8_t> sid;   // Sensor IDs (0-255)
};

struct Cluster {
    uint32_t id;
    uint8_t sensor_mask;
    float cx, cy, minx, miny, maxx, maxy;  // Centroid & bounding box
    std::vector<size_t> point_indices;
};

struct SinkConfig {
    std::string cluster_topic;  // OSC address / NNG topic prefix (default: "/hokuyohub/cluster")
    std::string raw_topic;      // OSC address / NNG topic prefix (default: "/hokuyohub/raw")
    int rate_limit;             // Max frames/sec (0=unlimited)
    bool send_clusters;         // Enable cluster publishing
    bool send_raw;              // Enable raw point publishing
    std::variant<OscConfig, NngConfig> cfg;
};
```

## REST API (v1)

All endpoints are prefixed with `/api/v1`. Mutating endpoints require Bearer token auth when `security.api_token` is set.

### Sensors
- `GET /api/v1/sensors` — List all sensors
- `GET /api/v1/sensors/<id>` — Get sensor by ID
- `POST /api/v1/sensors` — Add new sensor
- `PATCH /api/v1/sensors/<id>` — Update sensor
- `DELETE /api/v1/sensors/<id>` — Remove sensor

### Filters
- `GET /api/v1/filters` — List all filter configs
- `GET /api/v1/filters/prefilter` — Get prefilter config
- `PUT /api/v1/filters/prefilter` — Update prefilter
- `GET /api/v1/filters/postfilter` — Get postfilter config
- `PUT /api/v1/filters/postfilter` — Update postfilter

### DBSCAN
- `GET /api/v1/dbscan` — Get DBSCAN parameters
- `PUT /api/v1/dbscan` — Update DBSCAN parameters

### Sinks (Publishers)
- `GET /api/v1/sinks` — List all configured sinks
- `POST /api/v1/sinks` — Add new sink
- `PATCH /api/v1/sinks/<index>` — Update sink
- `DELETE /api/v1/sinks/<index>` — Remove sink

### Config Management
- `GET /api/v1/configs/list` — List available config files
- `POST /api/v1/configs/load` — Load config from file
- `POST /api/v1/configs/import` — Import config as JSON
- `POST /api/v1/configs/save` — Save current config
- `GET /api/v1/configs/export` — Export config as YAML

### Other
- `GET /api/v1/snapshot` — Get current state snapshot
- `GET /api/v1/health` — Health check

## Publishing

### NNG (pub/sub)
- Protocol: NNG pub0, default `tcp://0.0.0.0:5555`
- Topic filtering: topic string prepended to message as prefix
- Encoding: MessagePack (default) or JSON
- Cluster message: `{v:1, seq, t_ns, raw:false, items:[{id,cx,cy,minx,miny,maxx,maxy,n},...]}`
- Raw message: `{v:1, seq, t_ns, raw:true, points:[{x,y,sid},...]}`

### OSC (UDP)
- Uses `cluster_topic`/`raw_topic` directly as OSC address paths
- Supports bundle mode (`in_bundle`) with optional fragmentation (`bundle_fragment_size`)

## Platform Support

- macOS (ARM64) — development
- Linux ARM64 (Raspberry Pi 5) — production deployment
- Docker cross-compilation supported
