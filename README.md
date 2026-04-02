# HokuyoHub

**Real-time LiDAR sensor data processing and visualization platform for Hokuyo sensors**

HokuyoHub is a comprehensive solution for collecting, processing, and visualizing data from Hokuyo LiDAR sensors in real-time. Built with a C++20 backend (CrowCpp web framework) and JavaScript ES6+ frontend (Node.js proxy server), it features advanced DBSCAN clustering, a powerful web-based interface for sensor management, advanced data filtering, clustering analysis, and multi-format data publishing via NNG/OSC/REST/WebSocket - making it ideal for robotics applications, autonomous systems, and research environments.

## 🚀 Key Features

- **Multi-Sensor Support**: Connect and manage multiple Hokuyo LiDAR sensors simultaneously
- **Real-time Visualization**: Interactive web-based visualization with WebSocket communication, pan, zoom, and region-of-interest tools
- **Advanced Data Processing**: Built-in DBSCAN clustering with pre/post filtering (neighborhood, spike, outlier, intensity, isolation)
- **Flexible Data Publishing**: Export cluster and raw data via NNG (MessagePack/JSON) and OSC protocol with per-sink topic configuration
- **Independent Publishing Flags**: `send_clusters` and `send_raw` can be enabled simultaneously per sink
- **Interactive Configuration**: Live parameter tuning through modern JavaScript ES6+ web interface with Node.js proxy server
- **ROI Management**: Create include/exclude polygon regions for focused area monitoring with three-panel layout
- **Filter Pipeline**: Multi-stage filtering system (prefilter → world mask → clustering → postfilter)
- **Cross-Platform**: Runs on Linux, macOS, and supports ARM64/AMD64 multi-architecture deployment
- **Fast Build Times**: Uses CrowCpp header-only web framework for faster compilation

## 🏗️ Technical Architecture

HokuyoHub employs a modern, high-performance architecture designed for real-time applications:

### Core Technology Stack
- **Backend**: C++20 with CrowCpp (header-only web framework)
- **Frontend**: JavaScript ES6+ with Canvas API, Node.js Express proxy server
- **Real-time Communication**: WebSocket for low-latency data streaming
- **Data Processing**: Advanced DBSCAN clustering with configurable performance parameters
- **Build System**: CMake with multi-architecture support (ARM64/AMD64)

### Performance Characteristics
- **Memory Efficiency**: Optimized data structures and filtering pipelines
- **Network Protocol**: NNG with MessagePack/JSON for high-throughput data publishing
- **Deployment**: Production-ready Docker containers for ARM64

### Frontend Implementation
- **JavaScript**: Modern ES6+ with Canvas API for visualization
- **Proxy Server**: Node.js Express server manages backend process and proxies API/WebSocket
- **UI Layout**: Three-panel responsive design
- **Real-time Updates**: WebSocket-based parameter tuning and data streaming

## 🎯 Quick Start

### Prerequisites

- **System Requirements**: Linux or macOS
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
   ./scripts/build/build_with_presets.sh release --install
   ```
   
   **For Production (Raspberry Pi 5):**
   ```bash
   ./scripts/build/docker_cross_build.sh --build-all
   ```

3. **Configure your sensors**
Edit `configs/default.yaml` to match your sensor setup:
```yaml
sensors:
  - id: "sensor1"
    name: "front-lidar"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"
    pose: { tx: 0.0, ty: 0.0, theta: 0.0 }
    enabled: true
```

4. **Launch backend**
```bash
./dist/darwin-arm64/hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8081
```

5. **Launch frontend dev server** (in another terminal)
```bash
cd webui-server && npm start
```

6. **Open the web interface**
Navigate to `http://localhost:3000` in your browser

For detailed build instructions (cross-compilation, Docker, dependency management), see **[BUILD.md](BUILD.md)**.

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
   - **NNG**: High-performance messaging with MessagePack/JSON serialization (`tcp://0.0.0.0:5555`)
   - **OSC**: Open Sound Control protocol for real-time communication (UDP)
3. Configure per-sink topics (`cluster_topic`, `raw_topic`) and publishing flags (`send_clusters`, `send_raw`)
4. Both `send_clusters` and `send_raw` can be enabled simultaneously per sink

#### Message Format

|順序|項目名|型|内容|
|---|---|---|---|
|1|id|int32|クラスタの識別ID|
|2|t_ns|int64|Unixタイムスタンプ（ナノ秒単位）|
|3|seq|int32|キャプチャのシーケンス番号（フレーム番号）|
|4|cx|float32|クラスタの中心 X座標 (メートル)|
|5|cy|float32|クラスタの中心 Y座標 (メートル)|
|6|minx|float32|バウンディングボックスの最小 X|
|7|miny|float32|バウンディングボックスの最小 Y|
|8|maxx|float32|バウンディングボックスの最大 X|
|9|maxy|float32|バウンディングボックスの最大 Y|
|10|n|int32|クラスタに含まれる点（データポイント）の数|

#### Message Format (Raw)

Raw版: 生データを受け取りたい場合、sink の `send_raw` を true に設定します。OSC の場合はトピック末尾に `/raw` を付けたアドレス（例: /hokuyohub/raw）へ、NNG の場合は `"raw": true` フラグ付きのメッセージとして送信されます。`send_clusters` と `send_raw` は独立したフラグで、両方を同時に有効にできます。

|順序|項目名|型|内容|
|---|---|---|---|
|1|t_ns|int64|Unixタイムスタンプ（ナノ秒単位）|
|2|seq|int32|キャプチャのシーケンス番号（フレーム番号）|
|3|x|float32|点の X 座標 (メートル)|
|4|y|float32|点の Y 座標 (メートル)|
|5|sid|int32|センサ点の ID（0-255 を格納）|

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
  - type: nng
    url: tcp://0.0.0.0:5555
    encoding: msgpack            # or "json"
    cluster_topic: /hokuyohub/cluster  # NNG topic prefix / OSC address
    raw_topic: /hokuyohub/raw          # NNG topic prefix / OSC address
    rate_limit: 120              # Max frames/sec (0=unlimited)
    send_clusters: true          # Enable cluster publishing
    send_raw: false              # Enable raw point publishing
  
  - type: osc
    url: 127.0.0.1:10000
    in_bundle: true
    bundle_fragment_size: 0
    cluster_topic: /hokuyohub/cluster
    raw_topic: /hokuyohub/raw
    rate_limit: 120
    send_clusters: true
    send_raw: false
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

| 問題 | 対処 |
|------|------|
| センサーに接続できない | IP/ポート確認、`ping <sensor-ip>`、ケーブル・電源確認 |
| Web UIが表示されない | `ps aux \| grep hokuyo_hub` で起動確認、`lsof -i :8081` でポート確認 |
| クラスタリング結果が悪い | `eps_norm` を調整 (1.5-4.0)、`minPts` を増加、プレフィルタを有効化 |
| CPU使用率が高い | `skip_step` でダウンサンプリング、`interval` でスキャン頻度を低減 |
| ビルドが失敗する | `cmake -DDEPS_MODE=fetch --preset mac-release` で依存を再取得 |

詳細: **[docs/build/troubleshooting.md](docs/build/troubleshooting.md)**

## 📡 REST API (v1)

All endpoints are prefixed with `/api/v1`. Mutating endpoints require Bearer token auth when `security.api_token` is set.

```bash
# List sensors
curl http://localhost:8081/api/v1/sensors

# Get DBSCAN parameters
curl http://localhost:8081/api/v1/dbscan

# Update DBSCAN parameters
curl -X PUT http://localhost:8081/api/v1/dbscan \
  -H "Content-Type: application/json" \
  -d '{"eps_norm": 3.0, "minPts": 8}'

# Export current configuration as YAML
curl http://localhost:8081/api/v1/configs/export

# List all sinks
curl http://localhost:8081/api/v1/sinks

# Health check
curl http://localhost:8081/api/v1/health
```

### Full Endpoint List

- **Sensors**: `GET/POST /sensors`, `GET/PATCH/DELETE /sensors/<id>`
- **Filters**: `GET /filters`, `GET/PUT /filters/prefilter`, `GET/PUT /filters/postfilter`
- **DBSCAN**: `GET/PUT /dbscan`
- **Sinks**: `GET/POST /sinks`, `PATCH/DELETE /sinks/<index>`
- **Config**: `GET /configs/list`, `POST /configs/load`, `POST /configs/import`, `POST /configs/save`, `GET /configs/export`
- **Other**: `GET /snapshot`, `GET /health`

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

## 📚 Documentation

- **[Quick Start Guide](QUICK_START.md)** — 最速で動かす手順
- **[Build Guide](BUILD.md)** — 全プラットフォームのビルド手順、プリセット、依存管理
- **[Developer Guide](README-DEVELOPERS.md)** — アーキテクチャ、コードパターン、テスト戦略、貢献ガイドライン
- **[Troubleshooting](docs/build/troubleshooting.md)** — ビルド・ランタイムの問題解決

---

*HokuyoHub - Empowering real-time LiDAR applications with advanced processing and intuitive visualization.*
