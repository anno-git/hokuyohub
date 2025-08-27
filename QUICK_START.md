# HokuyoHub Quick Start Guide

**Get HokuyoHub up and running in minutes**

This guide helps you quickly build and run HokuyoHub on your preferred platform. For detailed build instructions, see [`BUILD.md`](BUILD.md).

## 🚀 Choose Your Platform

### 📱 macOS (Recommended for Development)

**Prerequisites:**
```bash
# Install Homebrew (if not already installed)
/bin/bash -c "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/HEAD/install.sh)"

# Install CMake
brew install cmake
```

**Quick Build:**
```bash
# Clone and build
git clone <repository-url>
cd HokuyoHub

# Build and install (5-10 minutes)
./scripts/build_with_presets.sh release --install

# Run HokuyoHub
cd dist/darwin-arm64
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080
```

**Open:** http://localhost:8080

### 🥧 Raspberry Pi 5 (Recommended for Production)

**On macOS (Cross-compilation):**
```bash
# One-time setup
./scripts/setup_cross_compile.sh

# Build for Raspberry Pi 5 (8-12 minutes)
./scripts/cross_build.sh --preset rpi-release --install

# Copy to Raspberry Pi
scp -r dist/linux-arm64/ pi@raspberrypi:~/hokuyo-hub/
```

**On Raspberry Pi:**
```bash
# Install runtime dependencies
sudo apt update
sudo apt install libyaml-cpp0.7 libnng1 libssl3 zlib1g

# Run HokuyoHub
cd ~/hokuyo-hub
chmod +x hokuyo_hub
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080
```

**Open:** http://raspberrypi.local:8080

### 🐳 Docker (Any Platform)

**Prerequisites:**
```bash
# Ensure Docker Desktop is installed and running
docker version
```

**Quick Build:**
```bash
# Build container (30-45 minutes first time)
./docker/build.sh build-all

# Extract artifacts
./scripts/extract_docker_artifacts.sh hokuyo-hub:latest

# Run locally
cd dist/linux-arm64
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080
```

**Or run in container:**
```bash
docker run -d -p 8080:8080 --name hokuyo-hub hokuyo-hub:latest
```

**Open:** http://localhost:8080

## ⚡ First Time Setup

### 1. Configure Your Sensors

Edit the configuration file for your setup:

**macOS:** [`dist/darwin-arm64/configs/default.yaml`](dist/darwin-arm64/configs/default.yaml)  
**Raspberry Pi/Docker:** [`dist/linux-arm64/configs/default.yaml`](dist/linux-arm64/configs/default.yaml)

```yaml
sensors:
  - id: "sensor1"
    name: "front-lidar"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"  # Replace with your sensor IP
    pose: { tx: 0.0, ty: 0.0, theta: 0.0 }
    enabled: true
```

### 2. Network Setup (For Ethernet Sensors)

```bash
# Configure network interface (example)
sudo ip addr add 192.168.1.10/24 dev eth0
sudo ip link set eth0 up

# Test connectivity
ping 192.168.1.100  # Replace with your sensor IP
```

### 3. Launch HokuyoHub

```bash
# Basic launch
./hokuyo_hub

# With custom config and listen address
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080

# With verbose logging
./hokuyo_hub --config ./configs/default.yaml --listen 0.0.0.0:8080 --verbose
```

### 4. Access Web Interface

1. Open your browser to http://localhost:8080
2. Click **"Add Sensor"** in the left panel
3. Enter your sensor configuration
4. Click **"Save"** to connect
5. View real-time data in the center visualization panel

## 🎮 Basic Usage

### Web Interface Overview

**Left Panel - Sensors & Sinks:**
- Add and configure LiDAR sensors  
- Set up data publishing endpoints
- Monitor connection status

**Center Panel - Visualization:**  
- Real-time point cloud display
- Interactive pan/zoom controls
- ROI (Region of Interest) creation

**Right Panel - Processing:**
- DBSCAN clustering parameters
- Pre/post-filtering options
- Live parameter adjustment

### Essential Controls

**Adding a Sensor:**
1. Click **"Add Sensor"**
2. Enter sensor details (name, IP, port)
3. Set position and orientation
4. Click **"Save"**

**Creating Regions of Interest:**
1. Click **"+ Include Region"** or **"+ Exclude Region"**
2. Click points on visualization to define polygon
3. Press **Enter** to complete
4. Use **"Clear All ROI"** to remove

**Data Publishing:**
1. Click **"Add Sink"** 
2. Choose format: NNG, OSC, or REST
3. Configure endpoint and topics

## 🔧 Quick Troubleshooting

### Sensor Won't Connect
```bash
# Check network connectivity
ping <sensor-ip>

# Verify port is accessible  
telnet <sensor-ip> <sensor-port>

# Check firewall settings
sudo ufw status  # Ubuntu/Debian
sudo /usr/libexec/ApplicationFirewall/socketfilterfw --getglobalstate  # macOS
```

### Web Interface Not Loading
```bash
# Check if service is running
ps aux | grep hokuyo_hub

# Verify port is available
netstat -ln | grep 8080
# or
lsof -i :8080

# Try different port
./hokuyo_hub --listen 0.0.0.0:8081
```

### Build Issues
```bash
# Clean and rebuild
./scripts/build_with_presets.sh clean
./scripts/build_with_presets.sh release --install

# Try different dependency mode
cmake -DDEPS_MODE=fetch --preset mac-release
cmake --build build/darwin-arm64
```

### Performance Issues
```bash
# Enable data downsampling in config
sensors:
  - skip_step: 2        # Use every 2nd data point
    interval: 100       # 100ms scan interval

# Reduce visualization rate
# Adjust in web interface settings
```

## 📚 Next Steps

**Explore Advanced Features:**
- **DBSCAN Clustering** - Automatic object detection
- **Multi-Sensor Setup** - Combine multiple LiDAR sensors  
- **Data Export** - NNG, OSC, and REST API publishing
- **ROI Management** - Focus on specific areas

**Read Documentation:**
- **[Main README](README.md)** - Complete feature guide
- **[Build Guide](BUILD.md)** - Detailed build instructions  
- **[Troubleshooting](docs/build/troubleshooting.md)** - Common issues and solutions

**Development:**
- **[macOS Build Guide](docs/build/macos.md)** - Development setup
- **[Advanced Configuration](docs/build/advanced.md)** - Custom builds

## 🆘 Getting Help

**Common Solutions:**
1. Check sensor IP/port configuration
2. Verify network connectivity  
3. Review application logs
4. Try different build configurations
5. Check system dependencies

**Log Analysis:**
```bash
# Run with verbose logging
./hokuyo_hub --config ./configs/default.yaml --verbose 2>&1 | tee hokuyo.log

# Monitor for key messages:
# - "[Sensor] Connected to ..." - Successful connection
# - "[DBSCAN] Processed frame ..." - Data processing  
# - "[Error]" - Issues to investigate
```

**Resources:**
- Project documentation in [`README.md`](README.md)
- Configuration examples in [`configs/`](configs/) directory  
- Build troubleshooting in [`docs/build/troubleshooting.md`](docs/build/troubleshooting.md)

---

**Quick Start Version:** 1.0  
**Compatible Platforms:** macOS ARM64, Raspberry Pi 5, Docker  
**Estimated Setup Time:** 10-45 minutes (depending on platform)

*Ready to build? Choose your platform above and follow the steps!*