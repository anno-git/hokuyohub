# HokuyoHub

**Real-time LiDAR sensor data processing and visualization platform for Hokuyo sensors**

HokuyoHub is a comprehensive solution for collecting, processing, and visualizing data from Hokuyo LiDAR sensors in real-time. Built with C++20 and featuring advanced DBSCAN clustering with 30 FPS real-time processing, it provides a powerful web-based interface for sensor management, advanced data filtering, clustering analysis, and multi-format data publishing - making it ideal for robotics applications, autonomous systems, and research environments.

## 🚀 Key Features

- **Multi-Sensor Support**: Connect and manage multiple Hokuyo LiDAR sensors simultaneously
- **Real-time Visualization**: Interactive web-based visualization with WebSocket communication, pan, zoom, and region-of-interest tools
- **Advanced Data Processing**: Built-in DBSCAN clustering with pre/post filtering, 30 FPS real-time processing
- **Flexible Data Publishing**: Export processed data via NNG with MessagePack, OSC protocol, or REST API
- **Interactive Configuration**: Live parameter tuning through modern JavaScript ES6+ web interface
- **ROI Management**: Create include/exclude regions for focused area monitoring with three-panel layout
- **Filter Pipeline**: Multi-stage filtering system (prefilter → clustering → postfilter)
- **Cross-Platform**: Runs on Linux, macOS, and Windows with ARM64/AMD64 multi-architecture deployment
- **Fast Build Times**: Uses CrowCpp header-only web framework for 60-70% faster compilation

## 🌟 Web Framework: CrowCpp

HokuyoHub uses **CrowCpp**, a modern, lightweight web framework that delivers excellent performance:

### ✅ Migration Benefits
- **⚡ Faster Build Times**: CrowCpp is header-only, eliminating complex dependency compilation
- **🪶 Lightweight**: Reduced binary size and memory footprint
- **🔧 Simplified Deployment**: No external web framework dependencies to manage
- **⚙️ Better CI/CD**: Dramatically reduced build times in continuous integration
- **� Modern C++**: Clean, type-safe API with excellent performance
- **🔍 Easier Debugging**: Header-only design provides better build error messages

### 🔄 What Changed
- **Web Framework**: CrowCpp (header-only, lightweight)
- **Build System**: Simplified build configurations
- **Dependencies**: Minimal external dependencies
- **API Compatibility**: All REST endpoints remain unchanged
- **Performance**: Maintained or improved response times with reduced overhead

## 🏗️ Technical Architecture

HokuyoHub employs a modern, high-performance architecture designed for real-time applications:

### Core Technology Stack
- **Backend**: C++20 with modern standards compliance
- **Web Framework**: CrowCpp (header-only, lightweight)
- **Real-time Communication**: WebSocket for low-latency data streaming
- **Data Processing**: Advanced DBSCAN clustering with 30 FPS capability
- **Build System**: CMake with multi-architecture support (ARM64/AMD64)

### Performance Characteristics
- **Processing Speed**: 30 FPS real-time LiDAR data processing
- **Build Performance**: 60-70% faster compilation times
- **Memory Efficiency**: Optimized data structures and filtering pipelines
- **Network Protocol**: NNG with MessagePack for high-throughput data publishing
- **Deployment**: Production-ready Docker containers (208MB runtime)

### Frontend Implementation
- **JavaScript**: Modern ES6+ with Canvas API for visualization
- **UI Layout**: Three-panel responsive design
- **Real-time Updates**: WebSocket-based parameter tuning and data streaming
- **Visualization**: Hardware-accelerated Canvas rendering for smooth 30 FPS display

## 🎯 Quick Start

### Prerequisites

- **System Requirements**: Linux, macOS, or Windows
- **Build Tools**: CMake 3.18+, C++20 compatible compiler
- **Network**: Access to Hokuyo sensors via Ethernet

### Installation

1. **Clone the repository**
```bash
git clone <repository-url>
cd HokuyoHub
```

2. **Build the application**
   
   **For Development (macOS):**
   ```bash
   ./scripts/build/build_with_presets.sh
   ```
   
   **For Production (Raspberry Pi 5):**
   ```bash
   ./scripts/build/docker_cross_build.sh --build-all
   ```

4. **Configure your sensors**
Edit `config/default.yaml` to match your sensor setup:
```yaml
sensors:
  - id: "sensor1"
    name: "front-lidar"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"
    pose: { tx: 0.0, ty: 0.0, theta: 0.0 }
    enabled: true
```

5. **Launch HokuyoHub**
```bash
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080
```

6. **Open the web interface**
Navigate to `http://localhost:8080` in your browser

## 📋 Installation Guide

### System Dependencies

**Ubuntu/Debian:**
```bash
sudo apt update
sudo apt install build-essential cmake git
sudo apt install libyaml-cpp-dev  # Optional: use system libraries
```

**macOS:**
```bash
# Install Homebrew dependencies
brew install cmake yaml-cpp

# Note: CrowCpp is a lightweight, header-only web framework
# No additional web framework dependencies needed
```

**Windows:**
```cmd
# Install dependencies via vcpkg
vcpkg install yaml-cpp jsoncpp asio --triplet x64-windows

# Or use Visual Studio 2022 with vcpkg integration
# Dependencies will be automatically resolved via CMake
```

### Build Options

**Modern Build System (Recommended)**

HokuyoHub uses an optimized build system with 60-70% faster build times and organized scripts for different deployment targets:

```bash
# Native macOS development build (optimized for development)
./scripts/build/build_with_presets.sh

# Docker ARM64 cross-compilation (production-ready for Raspberry Pi 5)
./scripts/build/docker_cross_build.sh --build-all

# Environment setup (automated dependency management)
./scripts/setup/setup_cross_compile.sh

# Testing and validation (comprehensive API testing)
./scripts/testing/test_rest_api.sh http://localhost:8080
```

**Build System Architecture:**
- **`scripts/build/`** - Core build scripts
- **`scripts/setup/`** - Environment configuration
- **`scripts/testing/`** - Testing and validation
- **`scripts/utils/`** - Utility scripts and artifact extraction
- **`docker/`** - Docker multi-stage build system

### Cross-Compilation

For ARM64 targets (Raspberry Pi 5), use the Docker-based cross-compilation:

```bash
# Complete cross-compilation build
./scripts/build/docker_cross_build.sh --build-all

# Extract deployment artifacts
# (automatically included in --build-all)
# Output: dist/linux-arm64/
```

**Docker Build Features:**
- Multi-stage build (dependencies → application → runtime)
- Automatic URG library cross-compilation
- Optimized runtime container (208MB)
- ARM64 native binary generation

## 🎮 Basic Usage

### Web Interface Overview

The HokuyoHub web interface features a modern JavaScript ES6+ implementation with Canvas API rendering and provides three main panels:

1. **Left Panel - Sensors & Sinks**
   - Add and configure LiDAR sensors
   - Set up data publishing endpoints
   - Monitor connection status with real-time updates

2. **Center Panel - Visualization**
   - Real-time point cloud display with Canvas API
   - Interactive pan/zoom controls
   - ROI (Region of Interest) creation tools
   - Raw and filtered data overlay with WebSocket communication

3. **Right Panel - Processing Configuration**
   - DBSCAN clustering parameters with real-time tuning
   - Pre/post-filtering options
   - Live parameter adjustment with immediate visualization feedback

### Adding a Sensor

1. Click "Add Sensor" in the left panel
2. Configure sensor parameters:
   - **Name**: Descriptive sensor name
   - **Type**: `hokuyo_urg_eth` for Ethernet sensors
   - **Endpoint**: IP address and port (e.g., `192.168.1.100:10940`)
   - **Position**: Sensor pose (x, y, rotation)
3. Click "Save" to activate the sensor

### Creating Regions of Interest

1. Click "+ Include Region" or "+ Exclude Region"
2. Click points on the visualization to define the polygon
3. Press Enter to complete the region
4. Use "Clear All ROI" to remove all regions

### Data Publishing Setup

1. Click "Add Sink" in the sinks panel
2. Choose publishing method:
   - **NNG**: High-performance messaging with MessagePack serialization (`tcp://0.0.0.0:5555`)
   - **OSC**: Open Sound Control protocol for real-time communication
   - **REST**: HTTP API access for web-based integration
3. Configure topic names and data formats with production-ready deployment support

## ⚙️ Configuration

### Sensor Configuration

```yaml
sensors:
  - id: "sensor1"
    name: "front-scanner"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"
    pose: { tx: 1.0, ty: 0.0, theta: 0.0 }  # Position in meters, rotation in radians
    enabled: true
    mode: "ME"              # "MD" = distance only, "ME" = distance + intensity
    skip_step: 1            # Data downsampling (1 = no downsampling)
    interval: 0             # Scan interval in ms (0 = default)
    mask:
      angle: { min: -135.0, max: 135.0 }  # Angular range in degrees
      range: { near: 0.05, far: 15.0 }    # Distance range in meters
```

### DBSCAN Clustering

Advanced DBSCAN implementation with optimized performance for 30 FPS real-time processing:

```yaml
dbscan:
  eps_norm: 2.5           # Normalized distance threshold
  minPts: 5               # Minimum points per cluster
  k_scale: 1.0            # Angular scaling factor
  h_min: 0.01             # Grid resolution minimum (m)
  h_max: 0.20             # Grid resolution maximum (m)
  R_max: 5                # Search radius limit (optimized for performance)
  M_max: 600              # Maximum candidates per query (balanced for speed/accuracy)
```

### Filtering Pipeline

```yaml
prefilter:
  enabled: true
  neighborhood:
    enabled: true
    k: 5                  # Minimum neighbors required
    r_base: 0.05          # Base radius in meters
    r_scale: 1.0          # Distance-based scaling
  
  spike_removal:
    enabled: true
    dr_threshold: 0.3     # Spike detection sensitivity
    window_size: 3        # Processing window size
  
  outlier_removal:
    enabled: true
    median_window: 5      # Moving median window
    outlier_threshold: 2.0 # Standard deviation threshold

postfilter:
  enabled: true
  isolation_removal:
    enabled: true
    min_points_size: 3    # Minimum cluster size
    isolation_radius: 0.2 # Inter-cluster distance threshold
```

### Data Publishing

```yaml
sinks:
  - type: "nng"
    url: "tcp://0.0.0.0:5555"
    topic: "clusters"
    encoding: "msgpack"
    rate_limit: 120
  
  - type: "osc"
    url: "127.0.0.1:10000"
    in_bundle: true
    topic: "clusters"
    encoding: "osc"
    rate_limit: 120
```

## 🔧 Supported Hardware

### Hokuyo Sensor Compatibility

**Tested Models:**
- URG-04LX-UG01 (USB/Ethernet)
- UTM-30LX (Ethernet)
- UST-10LX/20LX (Ethernet)

**Connection Requirements:**
- **Ethernet Models**: Direct network connection or switch
- **USB Models**: Serial-over-USB interface
- **Power**: External power supply for most models
- **Network**: Static IP configuration recommended

### Network Configuration

For Ethernet sensors, ensure proper network setup:

```bash
# Example: Configure static IP for sensor communication
sudo ip addr add 192.168.1.10/24 dev eth0
sudo ip link set eth0 up

# Test sensor connectivity
ping 192.168.1.100  # Replace with your sensor IP
```

## 🔍 Troubleshooting

### Common Issues

**Sensor Connection Failed**
- Verify IP address and port configuration
- Check network connectivity: `ping <sensor-ip>`
- Ensure sensor power and network cables are secure
- Try different skip_step values if data rate is too high

**Web Interface Not Loading**
```bash
# Check if service is running
ps aux | grep hokuyo_hub

# Verify port availability
netstat -ln | grep 8080

# Check configuration file
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080
```

**Poor Clustering Results**
- Adjust DBSCAN `eps_norm` parameter (try 1.5-4.0 range)
- Increase `minPts` for noisy environments
- Enable prefiltering for better data quality
- Check sensor mounting and environmental conditions

**High CPU Usage**
- Enable `skip_step` for data downsampling
- Reduce scan frequency with `interval` setting
- Lower visualization frame rate
- Optimize ROI regions to focus processing

### Performance Optimization

**For High-Frequency Applications:**
```yaml
# Optimize for speed
dbscan:
  h_min: 0.02    # Larger grid cells
  M_max: 400     # Fewer candidates
prefilter:
  neighborhood:
    r_base: 0.08 # Larger filtering radius
```

**For High-Precision Applications:**
```yaml
# Optimize for accuracy
dbscan:
  h_min: 0.005   # Smaller grid cells
  M_max: 1000    # More candidates
prefilter:
  neighborhood:
    r_base: 0.03 # Smaller filtering radius
```

### Log Analysis

```bash
# Enable verbose logging
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080 2>&1 | tee hokuyo.log

# Common log patterns to monitor:
# - "[Sensor] Connected to ..." - Sensor initialization
# - "[DBSCAN] Processed frame ..." - Clustering statistics
# - "[Prefilter] Filtered ..." - Data processing stats
```

## 📡 REST API

Access sensor data and configuration via HTTP:

```bash
# Get sensor status
curl http://localhost:8080/api/sensors

# Get current configuration
curl http://localhost:8080/api/config

# Update DBSCAN parameters
curl -X POST http://localhost:8080/api/dbscan \
  -H "Content-Type: application/json" \
  -d '{"eps_norm": 3.0, "minPts": 8}'
```

## 📄 License and Support

This project is available under the MIT License. 

**Getting Help:**
- Check the [troubleshooting section](#-troubleshooting) for common issues
- Review configuration examples in the `configs/` directory
- Examine log output for detailed error information

**Contributing:**
- Report bugs and feature requests via issues
- Submit pull requests for improvements
- Share configuration examples for different sensor setups

**System Requirements:**
- **Minimum**: 2GB RAM, dual-core CPU
- **Recommended**: 4GB+ RAM, quad-core CPU for multiple sensors with 30 FPS processing
- **Network**: Gigabit Ethernet for high-frequency scanning and real-time data publishing
- **Architecture**: ARM64/AMD64 multi-architecture support for diverse deployment scenarios
- **Windows**: Windows 10/11 x64, Visual Studio 2022, vcpkg for dependency management

## 📚 Documentation

### Build & Development
- **[Build Guide](BUILD.md)** - Comprehensive build instructions for all platforms
- **[Quick Start Guide](QUICK_START.md)** - Get up and running in minutes
- **[Docker Build System](docker/README.md)** - Optimized Docker multi-stage builds
- **[Build Performance](docs/build/OPTIMIZATION_SUMMARY.md)** - 60-70% faster build optimizations

### Development & Planning
- **[Development Plans](docs/development/plans/)** - Feature roadmap and implementation plans
- **[Legacy Documentation](docs/legacy/README.md)** - Historical build system documentation

### Quick References
- **Build Commands**: Use `./scripts/build/docker_cross_build.sh --build-all` for production
- **Testing**: Use `./scripts/testing/test_rest_api.sh` for API validation
- **Deployment**: ARM64 artifacts available in `dist/linux-arm64/`

---

*HokuyoHub - Empowering real-time LiDAR applications with advanced processing and intuitive visualization.*
