# HokuyoHub Developer Documentation

## Table of Contents
1. [Architecture Overview](#architecture-overview)
2. [Project Structure](#project-structure)
3. [Build System](#build-system)
4. [Development Environment Setup](#development-environment-setup)
5. [Code Organization](#code-organization)
6. [API Design](#api-design)
7. [Data Processing Pipeline](#data-processing-pipeline)
8. [Testing Strategy](#testing-strategy)
9. [Performance Considerations](#performance-considerations)
10. [Contributing Guidelines](#contributing-guidelines)
11. [Deployment](#deployment)
12. [Troubleshooting Development Issues](#troubleshooting-development-issues)

## Architecture Overview

HokuyoHub is a real-time sensor data processing and visualization system built with modern C++20 and a web-based frontend. The architecture follows a modular design pattern with clear separation of concerns:

```
┌─────────────────────────────────────────────────────────────┐
│                    Web Frontend (JS/HTML)                   │
├─────────────────────────────────────────────────────────────┤
│              HTTP REST API / WebSocket Layer                │
│                    (CrowCpp Framework)                      │
├─────────────────────────────────────────────────────────────┤
│                   Application Logic Layer                   │
│  ┌─────────────────┐  ┌────────────────┐  ┌──────────────┐ │
│  │ Sensor Manager  │  │ Filter Manager │  │ Publisher    │ │
│  │                 │  │                │  │ Manager      │ │
│  └─────────────────┘  └────────────────┘  └──────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                    Data Processing Layer                    │
│  ┌─────────────────┐  ┌────────────────┐  ┌──────────────┐ │
│  │   Clustering    │  │   Filtering    │  │ Coordinate   │ │
│  │ (DBSCAN2D)      │  │   Pipeline     │  │ Transform    │ │
│  └─────────────────┘  └────────────────┘  └──────────────┘ │
├─────────────────────────────────────────────────────────────┤
│                      Hardware Layer                         │
│                URG Library / Sensor Interface               │
└─────────────────────────────────────────────────────────────┘
```

### Key Design Patterns

- **Factory Pattern**: Used in [`SensorFactory`](src/sensors/SensorFactory.h:1) for creating different sensor types
- **Observer Pattern**: Implemented for real-time data publishing via WebSocket in [`LiveWs`](src/io/ws_handlers.h:14)
- **Strategy Pattern**: Filter algorithms are interchangeable via [`FilterManager`](src/core/filter_manager.h:10)
- **RAII Pattern**: Resource management for sensor connections and memory throughout the codebase
- **Dependency Injection**: Components receive dependencies through constructors (see [`main.cpp`](src/main.cpp:46))

### Data Flow

1. **Sensor Data Acquisition**: URG library communicates with Hokuyo sensors via [`HokuyoSensorUrg`](src/sensors/hokuyo/HokuyoSensorUrg.h:1)
2. **Raw Data Processing**: [`SensorManager`](src/core/sensor_manager.h:36) handles data acquisition and frame generation
3. **Filter Pipeline**: [`FilterManager`](src/core/filter_manager.h:10) applies prefiltering and postfiltering
4. **Clustering Analysis**: [`DBSCAN2D`](src/detect/dbscan.h:21) algorithm identifies point clusters
5. **ROI Filtering**: World mask filtering applied via [`world_mask`](src/main.cpp:102) configuration
6. **Real-time Publishing**: [`PublisherManager`](src/io/publisher_manager.h:1) broadcasts data to various sinks
7. **REST API**: [`RestApi`](src/io/rest_handlers.h:11) provides configuration management and system control

## Project Structure

```
HokuyoHub/
├── cmake/                      # CMake configuration and toolchains
│   ├── Dependencies.cmake     # Unified dependency management
│   ├── test_sources/          # Test configuration templates  
│   └── toolchains/            # Cross-compilation toolchains
├── configs/                   # Configuration files
│   └── default.yaml          # Default system configuration
├── docker/                    # Container deployment files (create as needed)
│   └── docker-compose.yml    # Multi-service orchestration (create as needed)
├── docs/                      # Project documentation
│   └── plans/                # Development planning documents
├── external/                  # External dependencies
│   └── urg_library/          # Hokuyo URG sensor library
├── scripts/                   # Build and utility scripts
│   └── test_rest_api.sh      # API testing utilities
├── src/                       # Source code
│   ├── detect/               # Detection and clustering algorithms
│   │   ├── dbscan.h/cpp      # DBSCAN clustering implementation
│   │   ├── prefilter.h/cpp   # Pre-clustering filters
│   │   └── postfilter.h/cpp  # Post-clustering filters
│   ├── config/               # Configuration management
│   │   ├── config.h/cpp      # Configuration structures and loading
│   ├── core/                 # Core system components
│   │   ├── sensor_manager.h/cpp    # Sensor lifecycle management
│   │   ├── filter_manager.h/cpp    # Filter pipeline coordination
│   │   └── mask.h/cpp              # ROI mask implementation
│   ├── sensors/              # Sensor abstraction layer
│   │   ├── ISensor.h         # Sensor interface
│   │   ├── SensorFactory.h/cpp     # Sensor creation factory
│   │   └── hokuyo/           # Hokuyo-specific implementation
│   │       ├── HokuyoSensorUrg.h/cpp # URG library integration
│   ├── io/                   # Input/Output handling
│   │   ├── rest_handlers.h/cpp     # REST API endpoints
│   │   ├── ws_handlers.h/cpp       # WebSocket communication
│   │   ├── publisher_manager.h/cpp # Data publishing coordination
│   │   ├── nng_bus.h/cpp          # NNG messaging bus
│   │   └── osc_publisher.h/cpp    # OSC protocol support
│   └── main.cpp              # Application entry point
├── third_party/               # Third-party dependencies
├── validation/               # Test data and validation scripts
├── webui/                    # Web frontend
│   ├── css/                 # Stylesheets
│   ├── js/                  # JavaScript modules
│   │   ├── api.js           # REST API client
│   │   ├── sensors.js       # Sensor visualization
│   │   └── utils.js         # Utility functions
│   └── index.html           # Main web interface
└── CMakeLists.txt           # Root CMake configuration
```

### Key Directories Explained

- **`src/core/`**: Contains the fundamental system components including sensor and filter management
- **`src/io/`**: Handles all external communication (REST API, WebSocket, messaging protocols)
- **`src/detect/`**: Implements clustering algorithms and filtering pipeline
- **`src/sensors/`**: Sensor abstraction layer with pluggable sensor implementations
- **`src/config/`**: Configuration management with YAML support
- **`webui/`**: Complete web frontend for real-time visualization and system control

## Build System

### CMake Configuration

The project uses modern CMake (3.18+) with C++20 standard and unified dependency management:

```cmake
# CMakeLists.txt key configuration
cmake_minimum_required(VERSION 3.18)
project(hokuyo_hub LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

# Unified dependency management
include(cmake/Dependencies.cmake)
resolve_all_dependencies()
```

### Build Targets

- **`hokuyo_hub`**: Main executable target
- **`sensor_core`**: Sensor abstraction library
- **`install`**: Installation target for deployment

### Dependency Management

The build system uses a unified dependency management approach via [`cmake/Dependencies.cmake`](cmake/Dependencies.cmake:1):

1. **CrowCpp Framework**: Modern header-only C++ web framework
   - Included as header-only dependency
   - Provides HTTP/WebSocket server capabilities
   - Thread-safe asynchronous I/O

2. **URG Library**: Hokuyo sensor communication
   - Bundled in `external/urg_library/`
   - Cross-platform sensor interface
   - Handles multiple sensor protocols

3. **Additional Dependencies**: 
   - **NNG**: Messaging library for inter-process communication
   - **OSC**: Open Sound Control protocol support (optional)
   - **JSON**: Configuration and API data handling

### Build Commands

```bash
# Standard build
mkdir build && cd build
cmake ..
make -j$(nproc)

# Debug build with testing
cmake -DCMAKE_BUILD_TYPE=Debug ..
make -j$(nproc)

# Cross-compilation example (Raspberry Pi)
cmake -DCMAKE_TOOLCHAIN_FILE=../cmake/toolchains/raspberry-pi.cmake ..
make -j$(nproc)

# Install to dist/ directory
make install
```

### Optional Features

```bash
# Build with OSC support (enabled by default)
cmake -DUSE_OSC=ON ..

# Disable OSC support
cmake -DUSE_OSC=OFF ..
```

## Development Environment Setup

### Prerequisites

**System Requirements:**
- C++20 compatible compiler (GCC 10+, Clang 10+, MSVC 2019+)
- CMake 3.18 or higher
- Git for source control with submodules
- Modern web browser for frontend testing

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake git libjsoncpp-dev uuid-dev zlib1g-dev
```

**macOS:**
```bash
brew install cmake jsoncpp ossp-uuid zlib
```

**Windows (Visual Studio):**
- Install Visual Studio 2019+ with C++ tools
- Install CMake via Visual Studio Installer
- Install vcpkg for dependency management

### IDE Configuration

**Visual Studio Code:**
```json
{
    "cmake.configureArgs": [
        "-DCMAKE_BUILD_TYPE=Debug",
        "-DUSE_OSC=ON"
    ],
    "C_Cpp.default.configurationProvider": "ms-vscode.cmake-tools",
    "C_Cpp.default.cppStandard": "c++20"
}
```

**CLion Configuration:**
- CMake Profile: Debug, Release
- Toolchain: System compiler or custom cross-compiler
- Build Directory: `build/`
- CMake Options: `-DUSE_OSC=ON`

### Development Workflow

1. **Clone with Submodules**: `git clone --recursive`
2. **Feature Branch**: Create feature branch from main
3. **Development**: Implement changes with proper testing
4. **Configuration**: Use YAML configuration files in `configs/`
5. **Testing**: Run integration tests with mock sensors
6. **Review**: Submit pull request for code review

## Code Organization

### Core Modules

#### SensorManager (`src/core/sensor_manager.h/cpp`)

Central component managing sensor lifecycle with multiple ID types:

```cpp
class SensorManager {
public:
    using FrameCallback = std::function<void(const ScanFrame&)>;
    
    SensorManager(AppConfig& app_config);
    void configure(const std::vector<SensorConfig>& cfgs);
    void start(FrameCallback cb);
    void setSensorPower(std::string sensor_id, bool on);
    void setPose(std::string sensor_id, float tx, float ty, float theta_deg);
    
private:
    AppConfig& app_config_;  // Reference to main config
};
```

**Sensor ID Management:**
- **String ID**: Human-readable identifiers for configuration (`"sensor_1"`)
- **Numeric ID (sid)**: `uint8_t` values (0-255) for high-performance point processing
- **Slot Index**: Array indices for API operations

#### FilterManager (`src/core/filter_manager.h/cpp`)

Orchestrates prefiltering and postfiltering pipeline with thread-safe operations:

```cpp
class FilterManager {
public:
    FilterManager(PrefilterConfig& prefilter_config, PostfilterConfig& postfilter_config);
    
    // Configuration management via JSON
    bool updatePrefilterConfig(const Json::Value& config);
    bool updatePostfilterConfig(const Json::Value& config);
    Json::Value getPrefilterConfigAsJson() const;
    Json::Value getPostfilterConfigAsJson() const;
    
    // Apply filters (thread-safe)
    // sid: 点群処理用の数値センサーID (0-255)
    Prefilter::FilterResult applyPrefilter(const std::vector<float>& xy, 
                                          const std::vector<uint8_t>& sid);
    Postfilter::FilterResult applyPostfilter(const std::vector<Cluster>& clusters,
                                           const std::vector<float>& xy,
                                           const std::vector<uint8_t>& sid);
    
    // Enable/disable checks
    bool isPrefilterEnabled() const { return prefilter_config_.enabled; }
    bool isPostfilterEnabled() const { return postfilter_config_.enabled; }
    
    // Configuration reload support
    void reloadFromAppConfig();
    
private:
    mutable std::mutex mutex_;
    PrefilterConfig& prefilter_config_;
    PostfilterConfig& postfilter_config_;
    std::unique_ptr<Prefilter> prefilter_;
    std::unique_ptr<Postfilter> postfilter_;
};
```

#### DBSCAN2D (`src/detect/dbscan.h/cpp`)

Advanced density-based clustering with sensor-aware distance metrics:

```cpp
struct Cluster {
    uint32_t id;                        // Unique cluster identifier
    uint8_t sensor_mask;               // Sensor participation mask
    float cx, cy;                      // Cluster centroid
    float minx, miny, maxx, maxy;      // Bounding box
    std::vector<size_t> point_indices; // Original point cloud indices
};

struct SensorModel {
    float delta_theta_rad;             // Angular resolution in radians
    float sigma0;                      // Distance noise constant term  
    float alpha;                       // Distance noise linear coefficient
};

class DBSCAN2D {
public:
    // Constructor with performance-optimized defaults
    DBSCAN2D(float eps, int minPts) : eps_(eps), minPts_(minPts), k_scale_(1.0f),
        h_min_(0.01f), h_max_(0.20f), R_max_(5), M_max_(600) {
        // Default sensor model (Δθ=0.25°, σ_r(r)=0.02+0.004·r)
        SensorModel default_model{0.0043633f, 0.02f, 0.004f};
        sensor_models_[0] = default_model;
    }
    
    // Parameter configuration
    void setParams(float eps, int minPts);
    void setAngularScale(float k_scale);
    void setSensorModel(uint8_t sid, float delta_theta_deg, float sigma0, float alpha);
    void setPerformanceParams(float h_min, float h_max, int R_max, int M_max);
    
    // Main clustering function with std::span for efficiency
    // minPts semantics are INCLUSIVE (neighbor count includes query point)
    std::vector<Cluster> run(std::span<const float> xy, 
                           std::span<const uint8_t> sid, 
                           uint64_t t_ns, uint32_t seq);
    
private:
    float eps_, k_scale_;
    int minPts_;
    std::unordered_map<uint8_t, SensorModel> sensor_models_;
    
    // Performance parameters
    float h_min_, h_max_;              // Grid cell size limits [m]
    int R_max_;                        // Maximum search radius in cells
    int M_max_;                        // Maximum candidate points per query
};
```

**Advanced Features:**
- **Sensor-Aware Clustering**: Different distance metrics per sensor using `SensorModel`
- **Angular Scaling**: `k_scale` parameter adjusts angular term influence (1.0 = theoretical optimum)
- **Performance Optimization**: Grid-based spatial indexing with configurable limits
- **Multi-Sensor Support**: Per-sensor noise models and parameters
- **Efficient Memory Layout**: Uses `std::span` for zero-copy data access

#### PublisherManager (`src/io/publisher_manager.h/cpp`)

Manages multiple data publishing sinks:

```cpp
class PublisherManager {
public:
    void publishClusters(uint64_t timestamp, uint32_t sequence,
                        const std::vector<Cluster>& clusters);
    
    // Configurable sinks: NNG, OSC, File, etc.
    void configureSinks(const std::vector<SinkConfig>& sinks);
};
```

### Data Structures

#### Core Data Types

```cpp
struct ScanFrame {
    uint64_t t_ns;                    // Timestamp in nanoseconds
    uint32_t seq;                     // Sequence number
    std::vector<float> xy;            // Point coordinates [x0,y0,x1,y1,...]
    std::vector<uint8_t> sid;         // Sensor IDs for each point (0-255)
};

struct Cluster {
    uint32_t id;                      // Unique cluster identifier
    uint8_t sensor_mask;             // Sensor participation bitmask
    float cx, cy;                    // Cluster centroid coordinates
    float minx, miny, maxx, maxy;    // Axis-aligned bounding box
    std::vector<size_t> point_indices; // Indices into original point cloud
};

// Filter result structures
namespace Prefilter {
    struct FilterResult {
        std::vector<float> xy;        // Filtered point coordinates
        std::vector<uint8_t> sid;     // Filtered sensor IDs
        struct {
            size_t input_points;      // Original point count
            size_t output_points;     // Filtered point count
            // Strategy-specific statistics...
        } stats;
    };
}

namespace Postfilter {
    struct FilterResult {
        std::vector<Cluster> clusters; // Filtered clusters
        struct {
            size_t input_clusters;    // Original cluster count
            size_t output_clusters;   // Filtered cluster count
            // Strategy-specific statistics...
        } stats;
    };
}
```

#### Configuration Structures

```cpp
struct AppConfig {
    std::vector<SensorConfig> sensors;    // Sensor configurations
    PrefilterConfig prefilter;           // Prefilter settings
    PostfilterConfig postfilter;         // Postfilter settings
    DbscanConfig dbscan;                 // Clustering parameters
    std::vector<SinkConfig> sinks;       // Output sink configurations
    SecurityConfig security;             // API authentication
    UiConfig ui;                         // Web UI settings
    core::WorldMask world_mask;          // ROI filtering
    
    // Legacy fields for backward compatibility
    float dbscan_eps{0.12f};
    int dbscan_minPts{6};
};

struct SensorConfig {
    std::string id;                      // Unique identifier
    std::string type{"hokuyo_urg_eth"};  // Sensor type
    std::string name{"sensor"};          // Display name
    std::string host{"192.168.1.10"};   // Network host
    int port{10940};                     // Network port
    bool enabled{true};                  // Enable/disable sensor
    
    // Acquisition settings
    std::string mode{"ME"};              // "MD"=distance only, "ME"=distance+intensity
    int interval{0};                     // Acquisition interval (ms, 0=default)
    int skip_step{0};                    // Step skipping for downsampling
    int ignore_checksum_error{1};        // Error handling
    
    PoseDeg pose;                        // Sensor pose (x, y, theta_deg)
    SensorMaskLocal mask;                // Local sensor mask (angle, range)
};

struct DbscanConfig {
    float eps{0.12f};                    // Legacy eps parameter
    float eps_norm{2.5f};               // Normalized distance threshold
    int minPts{5};                      // Minimum points for core cluster
    float k_scale{1.0f};                // Angular scaling coefficient
    
    // Performance optimization parameters
    float h_min{0.01f};                 // Minimum grid cell size [m]
    float h_max{0.20f};                 // Maximum grid cell size [m]
    int R_max{5};                       // Maximum search radius in cells
    int M_max{600};                     // Maximum candidate points per query
};
```

### Module Dependencies

```
main.cpp
├── SensorManager
│   ├── SensorFactory
│   │   └── HokuyoSensorUrg (URG Library)
│   └── AppConfig
├── FilterManager
│   ├── Prefilter
│   └── Postfilter
├── DBSCAN2D
├── RestApi (CrowCpp Framework)
│   ├── Sensor endpoints
│   ├── Filter endpoints
│   └── Configuration endpoints
├── LiveWs (WebSocket)
│   └── Real-time data streaming
└── PublisherManager
    ├── NNG Bus
    ├── OSC Publisher (optional)
    └── File Output
```

## API Design

### REST API Endpoints

Base URL: `http://localhost:8080/api/v1/`

Authentication: Bearer token via `Authorization` header (if configured)

#### Sensor Management

```cpp
// GET /api/v1/sensors
// Returns list of all configured sensors
Json::Value getSensors(const HttpRequestPtr& req);

// GET /api/v1/sensors/{id}
// Get specific sensor configuration
Json::Value getSensorById(const HttpRequestPtr& req);

// PATCH /api/v1/sensors/{id}
// Update sensor configuration (pose, power, mask, etc.)
void patchSensor(const HttpRequestPtr& req,
                std::function<void(const HttpResponsePtr&)>&& callback);

// POST /api/v1/sensors
// Add new sensor configuration
void postSensor(const HttpRequestPtr& req,
               std::function<void(const HttpResponsePtr&)>&& callback);

// DELETE /api/v1/sensors/{id}
// Remove sensor configuration
void deleteSensor(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback);
```

#### Filter Configuration

```cpp
// GET /api/v1/filters
// Get combined filter configuration
Json::Value getFilters(const HttpRequestPtr& req);

// GET /api/v1/filters/prefilter
// Get prefilter configuration
Json::Value getPrefilter(const HttpRequestPtr& req);

// PUT /api/v1/filters/prefilter
// Update prefilter configuration
void putPrefilter(const HttpRequestPtr& req,
                 std::function<void(const HttpResponsePtr&)>&& callback);

// GET /api/v1/filters/postfilter
// Get postfilter configuration
Json::Value getPostfilter(const HttpRequestPtr& req);

// PUT /api/v1/filters/postfilter  
// Update postfilter configuration
void putPostfilter(const HttpRequestPtr& req,
                  std::function<void(const HttpResponsePtr&)>&& callback);
```

#### Clustering Configuration

```cpp
// GET /api/v1/dbscan
// Get DBSCAN clustering parameters
Json::Value getDbscan(const HttpRequestPtr& req);

// PUT /api/v1/dbscan
// Update DBSCAN clustering parameters
void putDbscan(const HttpRequestPtr& req,
              std::function<void(const HttpResponsePtr&)>&& callback);
```

#### Configuration Management

```cpp
// GET /api/v1/configs/list
// List available configuration files
Json::Value getConfigsList(const HttpRequestPtr& req);

// POST /api/v1/configs/load
// Load configuration from file
void postConfigsLoad(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback);

// GET /api/v1/configs/export
// Export current configuration
Json::Value getConfigsExport(const HttpRequestPtr& req);

// POST /api/v1/configs/save
// Save current configuration to file
void postConfigsSave(const HttpRequestPtr& req,
                    std::function<void(const HttpResponsePtr&)>&& callback);
```

### WebSocket Protocols

#### Connection Endpoint

- **URL**: `ws://localhost:8080/ws/live`
- **Protocol**: JSON message-based communication
- **Authentication**: Token-based (if configured)

#### Message Types

**Sensor Updates:**
```javascript
// Sensor configuration update
{
    "type": "sensor_update",
    "sensor_id": "sensor_1",
    "enabled": true,
    "pose": {"x": 1.0, "y": 2.0, "theta": 90.0},
    "power": true
}

// Filter configuration update
{
    "type": "filter_update",
    "prefilter": {...},
    "postfilter": {...}
}

// DBSCAN parameter update
{
    "type": "dbscan_update",
    "eps": 0.1,
    "minPts": 3,
    "k_scale": 1.0
}
```

**Data Streaming:**
```javascript
// Raw sensor data
{
    "type": "raw_data",
    "timestamp": 1640995200000000000,
    "sequence": 12345,
    "points": [1.23, 4.56, 1.25, 4.58],  // [x0,y0,x1,y1,...]
    "sensor_ids": [1, 1, 2, 2]            // sensor ID per point
}

// Filtered sensor data
{
    "type": "filtered_data", 
    "timestamp": 1640995200000000000,
    "sequence": 12345,
    "points": [1.23, 4.56],               // After filtering
    "sensor_ids": [1, 1]
}

// Cluster results
{
    "type": "clusters",
    "timestamp": 1640995200000000000,
    "sequence": 12345,
    "clusters": [
        {
            "id": 1,
            "centroid": {"x": 1.24, "y": 4.57},
            "points": [[1.23, 4.56], [1.25, 4.58]],
            "confidence": 0.89
        }
    ]
}
```

**System Status:**
```javascript
// Complete system snapshot
{
    "type": "snapshot",
    "sensors": [...],     // All sensor configurations
    "filters": {...},     // Filter configurations
    "dbscan": {...},      // Clustering parameters
    "sinks": [...]        // Output sink configurations
}
```

## Data Processing Pipeline

### Pipeline Architecture

```
Raw Sensor Data (ScanFrame)
         ↓
┌─────────────────────────────────┐
│         Prefilter               │
│  ┌─────────┐  ┌─────────────┐   │
│  │ Range   │  │ Intensity   │   │
│  │ Filter  │  │ Filter      │   │  
│  └─────────┘  └─────────────┘   │
└─────────────────────────────────┘
         ↓
┌─────────────────────────────────┐
│      World Mask (ROI)           │
│   Applied via world_mask        │
│      configuration              │
└─────────────────────────────────┘
         ↓
┌─────────────────────────────────┐
│       DBSCAN2D Clustering       │
│   ┌─────────┐ ┌─────────────┐   │
│   │ Core    │ │ Border      │   │
│   │ Points  │ │ Assignment  │   │
│   └─────────┘ └─────────────┘   │
└─────────────────────────────────┘
         ↓
┌─────────────────────────────────┐
│         Postfilter              │
│  ┌─────────┐  ┌─────────────┐   │
│  │ Size    │  │ Confidence  │   │
│  │ Filter  │  │ Filter      │   │
│  └─────────┘  └─────────────┘   │
└─────────────────────────────────┘
         ↓
┌─────────────────────────────────┐
│       Publisher Manager         │
│  ┌─────────┐  ┌─────────────┐   │
│  │ WebSocket│  │ NNG/OSC     │   │
│  │ Clients  │  │ Sinks       │   │
│  └─────────┘  └─────────────┘   │
└─────────────────────────────────┘
```

### Filter Implementation

#### Prefilter Pipeline

Applied before clustering to reduce noise and computational load:

```cpp
Prefilter::FilterResult FilterManager::applyPrefilter(
    const std::vector<float>& xy,
    const std::vector<uint8_t>& sid) {
    
    Prefilter::FilterResult result;
    
    // Range-based filtering
    if (config_.range_filter.enabled) {
        applyRangeFilter(xy, sid, result);
    }
    
    // Intensity-based filtering  
    if (config_.intensity_filter.enabled) {
        applyIntensityFilter(result.xy, result.sid, result);
    }
    
    return result;
}
```

#### World Mask (ROI) Filtering

Geometric region-of-interest filtering applied in [`main.cpp`](src/main.cpp:102):

```cpp
// Apply ROI world_mask filtering after prefilter and before DBSCAN
if (!appcfg.world_mask.empty()) {
    std::vector<float> roi_filtered_xy;
    std::vector<uint8_t> roi_filtered_sid;
    
    for (size_t i = 0; i < filtered_xy.size(); i += 2) {
        if (i + 1 < filtered_xy.size()) {
            core::Point2D point(filtered_xy[i], filtered_xy[i + 1]);
            if (appcfg.world_mask.allows(point)) {
                roi_filtered_xy.push_back(filtered_xy[i]);
                roi_filtered_xy.push_back(filtered_xy[i + 1]);
                roi_filtered_sid.push_back(filtered_sid[i / 2]);
            }
        }
    }
    
    filtered_xy = std::move(roi_filtered_xy);
    filtered_sid = std::move(roi_filtered_sid);
}
```

#### DBSCAN Clustering Process

Enhanced DBSCAN with angular scaling and performance optimizations:

1. **Initialization**: Mark all points as unvisited
2. **Distance Calculation**: Apply angular scaling via `k_scale` parameter
3. **Core Point Detection**: Find points with sufficient neighbors within `eps`
4. **Cluster Formation**: Group connected core points using `minPts` threshold
5. **Border Assignment**: Add border points to nearest clusters
6. **Performance Limiting**: Apply `h_min`, `h_max`, `R_max`, `M_max` constraints

```cpp
std::vector<Cluster> DBSCAN2D::run(std::span<const float> xy,
                                  std::span<const uint8_t> sid,
                                  uint64_t timestamp,
                                  uint32_t sequence) {
    if (xy.size() < 2 || xy.size() % 2 != 0) return {};
    
    size_t n_points = xy.size() / 2;
    std::vector<int> labels(n_points, UNCLASSIFIED);
    std::vector<Cluster> clusters;
    
    for (size_t i = 0; i < n_points; ++i) {
        if (labels[i] != UNCLASSIFIED) continue;
        
        auto neighbors = regionQuery(i, xy);
        if (neighbors.size() < minPts_) {
            labels[i] = NOISE;
        } else {
            expandCluster(i, neighbors, clusters.size(), labels, xy, sid, clusters);
        }
    }
    
    return clusters;
}
```

#### Postfilter Pipeline

Applied after clustering to refine results:

```cpp
Postfilter::FilterResult FilterManager::applyPostfilter(
    const std::vector<Cluster>& clusters,
    const std::vector<float>& xy,
    const std::vector<uint8_t>& sid) {
    
    Postfilter::FilterResult result;
    
    for (const auto& cluster : clusters) {
        // Size-based filtering
        if (cluster.point_indices.size() >= config_.min_cluster_size &&
            cluster.point_indices.size() <= config_.max_cluster_size) {
            
            // Confidence-based filtering
            if (cluster.confidence >= config_.min_confidence) {
                result.clusters.push_back(cluster);
            }
        }
    }
    
    return result;
}
```

### Performance Optimizations

- **Vector Operations**: Efficient memory layout with `std::vector<float>` for coordinates
- **Sensor ID Optimization**: `uint8_t` sensor IDs for minimal memory footprint
- **Spatial Indexing**: Optimized neighbor searches for DBSCAN
- **Memory Reuse**: Preallocated buffers for frequent operations
- **SIMD Potential**: Data layout supports vectorized distance calculations

## Testing Strategy

### Unit Tests

The project uses a test-driven approach with modular unit testing:

```cpp
// Example test for DBSCAN clustering
TEST_F(DBSCANTest, BasicClusterFormation) {
    // Create test point cloud with known clusters
    std::vector<float> points = {
        1.0f, 1.0f,  1.1f, 1.0f,  1.0f, 1.1f,  // Cluster 1
        5.0f, 5.0f,  5.1f, 5.0f,  5.0f, 5.1f   // Cluster 2
    };
    std::vector<uint8_t> sids = {1, 1, 1, 2, 2, 2};
    
    DBSCAN2D clusterer(0.2f, 2);
    auto clusters = clusterer.run(points, sids, 1000, 1);
    
    EXPECT_EQ(clusters.size(), 2);
    EXPECT_EQ(clusters[0].point_indices.size(), 3);
    EXPECT_EQ(clusters[1].point_indices.size(), 3);
}
```

### Integration Tests

End-to-end pipeline testing with mock sensors and real configurations:

```cpp
TEST_F(IntegrationTest, CompletePipeline) {
    // Load test configuration
    AppConfig config = load_app_config("test_configs/integration_test.yaml");
    
    // Setup components
    SensorManager sensors(config);
    FilterManager filters(config.prefilter, config.postfilter);
    DBSCAN2D dbscan(config.dbscan.eps, config.dbscan.minPts);
    
    // Test complete data flow
    bool received_clusters = false;
    auto callback = [&](const ScanFrame& frame) {
        auto filtered = filters.applyPrefilter(frame.xy, frame.sid);
        auto clusters = dbscan.run(filtered.xy, filtered.sid, frame.t_ns, frame.seq);
        received_clusters = !clusters.empty();
    };
    
    sensors.start(callback);
    // Wait for data processing
    EXPECT_TRUE(received_clusters);
}
```

### API Testing

REST API validation using shell scripts in [`scripts/testing/test_rest_api.sh`](scripts/testing/test_rest_api.sh:1):

```bash
#!/bin/bash
# API endpoint testing

BASE_URL="http://localhost:8080/api/v1"
AUTH_TOKEN="your-api-token"

# Test sensor listing
echo "Testing sensor listing..."
response=$(curl -s -H "Authorization: Bearer $AUTH_TOKEN" "$BASE_URL/sensors")
echo "Sensors: $response"

# Test configuration export
echo "Testing configuration export..."
config=$(curl -s -H "Authorization: Bearer $AUTH_TOKEN" "$BASE_URL/configs/export")
echo "Config exported successfully"

# Test DBSCAN parameter update
echo "Testing DBSCAN parameter update..."
curl -X PUT -H "Authorization: Bearer $AUTH_TOKEN" \
     -H "Content-Type: application/json" \
     -d '{"eps": 0.15, "minPts": 4}' \
     "$BASE_URL/dbscan"
```

### Configuration Testing

Test various configuration scenarios with YAML files in `configs/`. Example [`default.yaml`](configs/default.yaml:1):

```yaml
sensors:
  - id: "sensor1"
    name: "hokuyo-a"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.104.202:10940"
    pose: { tx: 0.0, ty: 0.0, theta: 0.0 }
    enabled: true
    mode: "ME"          # "MD"=distance only / "ME"=distance+intensity
    skip_step: 1        # 1=no downsampling
    interval: 0         # 0=default (ms)
    mask:
      angle: { min: -135.0, max: 135.0 }  # degrees
      range: { near: 0.05, far: 15.0 }    # meters

world_mask:
  polygon: []   # [[x,y], ...] - ROI polygon vertices

dbscan:
  eps_norm: 5           # Normalized distance threshold
  minPts: 5             # Minimum points for core (inclusive of self)
  k_scale: 1            # Angular term scale coefficient
  
  # Performance parameters
  h_min: 0.01           # Minimum grid cell size [m]
  h_max: 0.20           # Maximum grid cell size [m]
  R_max: 5              # Maximum search radius in cells
  M_max: 600            # Maximum candidate points per query

prefilter:
  enabled: true
  
  neighborhood:
    enabled: true
    k: 5                # Minimum neighbors required
    r_base: 0.05        # Base radius [m]
    r_scale: 1.0        # Distance-based radius scaling factor
  
  spike_removal:
    enabled: false
    dr_threshold: 0.3   # |dR/dθ| threshold for spike detection
    window_size: 3      # Window size for derivative calculation
  
  outlier_removal:
    enabled: true
    median_window: 5    # Window size for moving median
    outlier_threshold: 2.0  # Threshold in standard deviations
    use_robust_regression: false
  
  intensity_filter:
    enabled: false      # Disabled by default (sensor dependent)
    min_intensity: 0.0  # Minimum intensity threshold
    min_reliability: 0.0 # Minimum reliability threshold
  
  isolation_removal:
    enabled: true
    min_cluster_size: 3 # Minimum points to keep a cluster
    isolation_radius: 0.1 # Radius to check for isolation

postfilter:
  enabled: true
  
  isolation_removal:
    enabled: true
    min_points_size: 3  # Minimum points to keep a cluster
    isolation_radius: 0.2 # Radius to check for isolation between clusters
    required_neighbors: 2 # Minimum neighbors required for a point

sinks:
  - { type: "nng", url: "tcp://0.0.0.0:5555", topic: "clusters", encoding: "msgpack", rate_limit: 120 }
  - { type: "osc", url: "127.0.0.1:10000", in_bundle: true, topic: "clusters", encoding: "osc", rate_limit: 120 }

ui:
  listen: "0.0.0.0:8080"
```

### WebSocket Testing

Real-time communication testing using JavaScript test clients:

```javascript
// WebSocket integration test
const ws = new WebSocket('ws://localhost:8080/ws/live');

ws.onmessage = (event) => {
    const data = JSON.parse(event.data);
    
    switch(data.type) {
        case 'raw_data':
            console.log(`Received ${data.points.length/2} raw points`);
            break;
        case 'clusters':
            console.log(`Received ${data.clusters.length} clusters`);
            break;
        case 'snapshot':
            console.log('System snapshot received');
            break;
    }
};

// Test sensor update
ws.send(JSON.stringify({
    type: 'sensor_update',
    sensor_id: 'test_sensor',
    enabled: false
}));
```

## Performance Considerations

### Memory Management

- **RAII Pattern**: Automatic resource cleanup throughout the codebase
- **Vector Optimization**: Efficient `std::vector` usage with reserved capacity
- **Minimal Copying**: Move semantics and reference passing where possible
- **Stack Allocation**: Prefer stack over heap for temporary objects

```cpp
// Example of efficient memory management in data processing
class DataProcessor {
    std::vector<float> processing_buffer_;    // Reused buffer
    std::vector<uint8_t> sid_buffer_;        // Reused sensor ID buffer
    
public:
    void processFrame(const ScanFrame& frame) {
        // Reserve capacity to avoid reallocations
        processing_buffer_.reserve(frame.xy.size());
        sid_buffer_.reserve(frame.sid.size());
        
        // Process without unnecessary copying
        processInPlace(processing_buffer_, sid_buffer_);
        
        // Clear but keep capacity for next frame
        processing_buffer_.clear();
        sid_buffer_.clear();
    }
};
```

### Threading Architecture

- **Main Thread**: HTTP server and application control via CrowCpp framework
- **Sensor Threads**: Individual threads per sensor for data acquisition
- **Processing Thread**: Filter pipeline and clustering in callback context
- **WebSocket Threads**: Real-time data broadcasting managed by CrowCpp
- **Publisher Threads**: Background publishing to external sinks (NNG, OSC)

### Real-time Performance

**Target Performance:**
- **Sensor Acquisition**: Up to 40Hz per sensor
- **Processing Latency**: < 10ms per frame
- **WebSocket Streaming**: 30Hz to web clients
- **Memory Usage**: < 100MB for typical configurations

**Optimization Techniques:**
1. **Vectorized Operations**: Data layout supports SIMD operations
2. **Spatial Indexing**: Efficient neighbor searches in DBSCAN
3. **Configuration Caching**: Avoid repeated parsing of settings
4. **Buffer Reuse**: Preallocated working buffers

### Profiling and Monitoring

**Performance Profiling:**
```bash
# CPU profiling with perf
perf record -g ./hokuyo_hub --config configs/performance_test.yaml
perf report -g graph,0.5,caller

# Memory profiling with Valgrind
valgrind --tool=massif ./hokuyo_hub
ms_print massif.out.* | head -30
```

**Runtime Monitoring:**
- Built-in performance statistics via WebSocket
- Processing time measurements in filter pipeline
- Memory usage tracking in sensor manager
- Cluster quality metrics in DBSCAN output

## Contributing Guidelines

### Code Style

Following modern C++20 best practices with project-specific conventions:

```cpp
// Class names: PascalCase
class SensorManager {
public:
    // Public methods: camelCase
    void startCapture();
    bool isConnected() const;
    
private:
    // Private members: snake_case with trailing underscore
    std::unique_ptr<ISensor> sensor_;
    mutable std::mutex data_mutex_;
    std::atomic<bool> capturing_{false};
};

// Constants: UPPER_CASE or constexpr
static constexpr int MAX_RETRY_COUNT = 3;
static const std::string DEFAULT_CONFIG_PATH = "./configs/default.yaml";

// Enums: PascalCase with scoped enumerators
enum class SensorType {
    HOKUYO_URG,
    MOCK_SENSOR
};
```

### Git Workflow

1. **Feature Branches**: `feature/description` or `fix/issue-number`
2. **Commit Messages**: Conventional Commits format
   ```
   feat(sensor): add multi-sensor pose configuration
   
   - Implement pose transformation in SensorManager
   - Add REST API endpoints for pose updates
   - Update WebSocket protocol for real-time pose changes
   
   Closes #123
   ```

3. **Pull Request Requirements**:
   - Description of changes and motivation
   - Testing performed (unit, integration, manual)
   - Performance impact assessment
   - Documentation updates
   - Breaking changes noted

### Development Setup

```bash
# Clone with submodules for external dependencies
git clone --recursive https://github.com/yourusername/HokuyoHub.git
cd HokuyoHub

# Setup development build
mkdir build-dev && cd build-dev
cmake -DCMAKE_BUILD_TYPE=Debug -DUSE_OSC=ON ..
make -j$(nproc)

# Run with test configuration
./hokuyo_hub --config ../configs/development.yaml --listen localhost:8080
```

### Code Review Checklist

- [ ] Code follows project style guidelines
- [ ] Unit tests added for new functionality
- [ ] Integration tests pass with configuration changes
- [ ] API changes documented in README-DEVELOPERS.md
- [ ] Performance impact assessed and documented
- [ ] Memory leaks checked with Valgrind
- [ ] Thread safety verified for concurrent access
- [ ] Configuration compatibility maintained
- [ ] WebSocket protocol changes documented

## Deployment

### Docker Configuration

**Note**: Docker configuration files are not currently included in the repository but can be created as needed for deployment.

**Recommended Production Dockerfile:**
```dockerfile
FROM ubuntu:22.04 AS builder

# Install build dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    cmake \
    git \
    libjsoncpp-dev \
    uuid-dev \
    zlib1g-dev \
    && rm -rf /var/lib/apt/lists/*

# Build application
COPY . /app
WORKDIR /app
RUN mkdir build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Release -DUSE_OSC=ON .. && \
    make -j$(nproc) && \
    make install

# Runtime image
FROM ubuntu:22.04

RUN apt-get update && apt-get install -y \
    libjsoncpp1 \
    libstdc++6 \
    && rm -rf /var/lib/apt/lists/*

# Copy application and assets from dist/ directory
COPY --from=builder /app/dist/hokuyo_hub /usr/local/bin/
COPY --from=builder /app/dist/configs /usr/local/etc/hokuyo_hub
COPY --from=builder /app/dist/webui /usr/local/share/hokuyo_hub/webui

# Runtime configuration
ENV HOKUYO_CONFIG=/usr/local/etc/hokuyo_hub/default.yaml

EXPOSE 8080
VOLUME ["/usr/local/etc/hokuyo_hub"]

CMD ["hokuyo_hub", "--config", "/usr/local/etc/hokuyo_hub/default.yaml", "--listen", "0.0.0.0:8080"]
```

**Docker Compose for Development:**
```yaml
version: '3.8'
services:
  hokuyo_hub:
    build: .
    ports:
      - "8080:8080"
    volumes:
      - ./configs:/usr/local/etc/hokuyo_hub
      - ./validation:/app/validation
    devices:
      - "/dev/ttyACM0:/dev/ttyACM0"  # USB sensor connection (if using USB sensors)
    environment:
      - USE_OSC=ON
    networks:
      - hokuyo_network
    
networks:
  hokuyo_network:
    driver: bridge
```

### Production Considerations

1. **Security**: 
   - Enable API token authentication via `security.api_token`
   - Use HTTPS reverse proxy (nginx, Apache)
   - Restrict network access to sensor ports
   - Validate all configuration inputs

2. **Monitoring**: 
   - Health check endpoints via REST API
   - Performance metrics via WebSocket
   - Log aggregation and analysis
   - Resource usage monitoring

3. **Scalability**: 
   - Multiple sensor instances with different configurations
   - Load balancing for web interface access
   - Horizontal scaling of processing components
   - External message bus integration (NNG)

4. **Reliability**: 
   - Automatic service restart with systemd
   - Configuration backup and recovery
   - Sensor connection retry logic
   - Graceful degradation on component failures

### Systemd Service Configuration

```ini
# /etc/systemd/system/hokuyo-hub.service
[Unit]
Description=HokuyoHub Sensor Processing Server
After=network.target
Wants=network.target

[Service]
Type=simple
User=hokuyo
Group=hokuyo
WorkingDirectory=/opt/hokuyo-hub
ExecStart=/opt/hokuyo-hub/hokuyo_hub --config /etc/hokuyo-hub/production.yaml --listen 0.0.0.0:8080
Restart=always
RestartSec=5
StandardOutput=journal
StandardError=journal

# Security settings
NoNewPrivileges=true
PrivateTmp=true
ProtectSystem=strict
ProtectHome=true
ReadWritePaths=/var/log/hokuyo-hub /etc/hokuyo-hub

[Install]
WantedBy=multi-user.target
```

## Troubleshooting Development Issues

### Build Issues

**CMake Configuration Problems:**
```bash
# Clear CMake cache completely
rm -rf build/
mkdir build && cd build

# Verbose configuration for debugging
cmake -DCMAKE_VERBOSE_MAKEFILE=ON ..

# Check dependency resolution
cmake --build . --target help
```

**Missing Dependencies:**
```bash
# Ubuntu: Install all required development packages
sudo apt-get install build-essential cmake git libjsoncpp-dev uuid-dev zlib1g-dev

# macOS: Use Homebrew for dependencies
brew install cmake jsoncpp ossp-uuid zlib

# Check pkg-config resolution
pkg-config --cflags --libs jsoncpp
```

**Submodule Issues:**
```bash
# Ensure all submodules are properly initialized
git submodule update --init --recursive

# Reset submodules if corrupted
git submodule deinit --all -f
git submodule update --init --recursive
```

### Runtime Issues

**Sensor Connection Problems:**
1. **Permission Issues**: 
   ```bash
   # Add user to dialout group for serial port access
   sudo usermod -a -G dialout $USER
   # Logout and login for group changes to take effect
   ```

2. **Device Detection**: 
   ```bash
   # Check available serial devices
   ls -la /dev/ttyACM* /dev/ttyUSB*
   
   # Test URG library directly
   cd external/urg_library/current/samples/c/get_distance
   make && ./get_distance
   ```

3. **Configuration Validation**: 
   ```bash
   # Validate YAML configuration syntax
   python3 -c "import yaml; yaml.safe_load(open('configs/default.yaml'))"
   ```

**Web Interface Issues:**
1. **Port Conflicts**: 
   ```bash
   # Check if port 8080 is already in use
   netstat -tlnp | grep 8080
   lsof -i :8080
   ```

2. **WebSocket Connection Problems**: 
   ```bash
   # Test WebSocket connectivity
   curl -i -N -H "Connection: Upgrade" -H "Upgrade: websocket" \
        -H "Sec-WebSocket-Version: 13" -H "Sec-WebSocket-Key: test" \
        http://localhost:8080/ws/live
   ```

3. **CORS Issues**: Check browser developer tools for cross-origin errors

**Performance Issues:**
1. **High CPU Usage**: 
   ```bash
   # Monitor per-thread CPU usage
   top -H -p $(pgrep hokuyo_hub)
   
   # Profile with perf
   perf top -p $(pgrep hokuyo_hub)
   ```

2. **Memory Leaks**: 
   ```bash
   # Run with AddressSanitizer
   cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_CXX_FLAGS="-fsanitize=address" ..
   make && ./hokuyo_hub --config configs/test.yaml
   
   # Detailed memory analysis
   valgrind --leak-check=full --show-leak-kinds=all ./hokuyo_hub
   ```

3. **Real-time Performance**: 
   ```bash
   # Monitor processing latency via WebSocket messages
   # Check for frame drops in clustering pipeline
   # Verify filter configuration optimization
   ```

### Configuration Issues

**YAML Parsing Errors:**
```bash
# Validate YAML syntax
yaml-lint configs/default.yaml

# Check configuration loading
./hokuyo_hub --config configs/debug.yaml --listen localhost:8081

# Test configuration loading with debug output
./hokuyo_hub --config configs/default.yaml --listen localhost:8081
```

**Filter Parameter Tuning:**
```yaml
# Example optimized configuration for real-time processing
prefilter:
  enabled: true
  range_filter:
    enabled: true
    min_range: 0.05    # Remove very close points
    max_range: 8.0     # Sensor max range
    
dbscan:
  eps: 0.12           # Cluster tolerance (meters)
  minPts: 3           # Minimum cluster size
  k_scale: 0.8        # Angular scaling factor
  h_min: 0.02         # Min cluster height
  h_max: 2.5          # Max cluster height
  R_max: 10.0         # Max processing radius
  M_max: 1000         # Max points per frame
```

### Debugging Techniques

**GDB Session Setup:**
```bash
# Compile with debug symbols
cmake -DCMAKE_BUILD_TYPE=Debug ..
make

# GDB with configuration
gdb ./hokuyo_hub
(gdb) set environment HOKUYO_CONFIG=configs/debug.yaml
(gdb) break SensorManager::start
(gdb) run --config configs/debug.yaml --listen localhost:8080
(gdb) bt  # backtrace on crash
(gdb) info threads  # thread information
```

**Advanced Debugging:**
```bash
# Core dump analysis
ulimit -c unlimited
./hokuyo_hub --config configs/test.yaml
# After crash:
gdb ./hokuyo_hub core

# Thread debugging with helgrind
valgrind --tool=helgrind ./hokuyo_hub --config configs/test.yaml
```

### Common Issues and Solutions

| Issue | Symptom | Solution |
|-------|---------|----------|
| Build fails with missing dependencies | CMake can't find required libraries | Check system packages and dependencies |
| Sensor timeout errors | Connection failures, no data | Check USB cable, permissions, device path |
| High CPU usage | Poor real-time performance | Optimize filter parameters, reduce processing load |
| Memory growth | Increasing memory usage over time | Enable AddressSanitizer, check buffer management |
| WebSocket disconnections | Intermittent client connections | Increase buffer sizes, check network stability |
| Clustering errors | Empty or invalid cluster results | Validate DBSCAN parameters, check input data |
| Configuration not loading | Default values used instead | Verify YAML syntax, file permissions, path |

### Log Analysis

**Application Logging:**
```bash
# Enable debug logging
export HOKUYO_LOG_LEVEL=DEBUG
./hokuyo_hub --config configs/debug.yaml 2>&1 | tee debug.log

# Filter specific component logs
grep "DBSCAN" debug.log
grep "Sensor" debug.log  
grep "Filter" debug.log
```

**System Resource Monitoring:**
```bash
# Monitor resource usage
iostat -x 1        # I/O statistics
vmstat 1           # Virtual memory statistics  
sar -u 1           # CPU utilization
```

For additional support and troubleshooting assistance, please check the GitHub Issues page or contact the development team through the project's communication channels.