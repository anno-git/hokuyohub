# Raspberry Pi 5 Build Guide

**Cross-compilation and deployment instructions for Raspberry Pi 5 (ARM64 Linux)**

This guide covers cross-compilation from macOS to Raspberry Pi 5, using the sophisticated cross-compilation system built into HokuyoHub. For other build methods, see the [main BUILD.md](../../BUILD.md).

## 🚀 Quick Start

**On macOS (Development Machine):**
```bash
# One-time setup
./scripts/setup_cross_compile.sh

# Build for Raspberry Pi 5
./scripts/cross_build.sh --preset rpi-release --install

# Copy to Raspberry Pi
scp -r dist/linux-arm64/ pi@raspberrypi:~/hokuyo-hub/
```

**On Raspberry Pi 5:**
```bash
# Install dependencies and run
sudo apt update && sudo apt install libyaml-cpp0.7 libnng1 libssl3 zlib1g
cd ~/hokuyo-hub && chmod +x hokuyo_hub && ./hokuyo_hub
```

## 🎯 Target Platform

### Raspberry Pi 5 Specifications
- **CPU:** Broadcom BCM2712 (Cortex-A76 quad-core)
- **Architecture:** ARM64 (aarch64)
- **RAM:** 4GB/8GB LPDDR4X
- **OS:** Raspberry Pi OS (64-bit) / Ubuntu Server ARM64

### Optimizations
- **CPU Targeting:** Cortex-A76 specific optimizations (`-mcpu=cortex-a76`)
- **Memory:** Efficient ARM64 instruction set usage
- **I/O:** Optimized for Ethernet-based sensor communication

## 📋 Prerequisites

### Development Machine (macOS)
- **macOS** Monterey+ with Xcode Command Line Tools
- **Homebrew** package manager
- **CMake** 3.18+ 
- **Cross-compilation toolchain** (installed via setup script)

### Target System (Raspberry Pi 5)
- **Raspberry Pi OS** 64-bit (Bookworm or newer)
- **Network access** for dependency installation
- **Ethernet connection** for Hokuyo sensors

## 🛠️ Cross-Compilation Setup

### Method 1: Automated Setup (Recommended)

```bash
# Run the setup script
./scripts/setup_cross_compile.sh

# Follow prompts to install missing tools
# The script will:
# - Check for existing toolchains
# - Install via Homebrew if needed  
# - Verify installation
# - Show usage examples
```

### Method 2: Manual Setup

**1. Install Cross-Compilation Toolchain:**

**Option A - GNU Toolchain (Recommended):**
```bash
brew install aarch64-linux-gnu-gcc
```

**Option B - musl Toolchain:**
```bash
brew install musl-cross
```

**2. Verify Installation:**
```bash
# Check tools are available
aarch64-linux-gnu-gcc --version
aarch64-linux-gnu-g++ --version
```

**3. Optional - Set Environment Variables:**
```bash
# Override toolchain prefix (optional)
export CROSS_COMPILE_PREFIX="aarch64-linux-gnu-"

# Set target sysroot for better library detection (optional)
export TARGET_SYSROOT="/path/to/raspberry-pi-sysroot"

# Set toolchain root (optional)
export TOOLCHAIN_ROOT="/opt/homebrew"
```

## 🔧 Build Methods

### Method 1: Cross-Build Script (Recommended)

The cross-build script provides a convenient interface for all cross-compilation tasks:

```bash
# Build release version
./scripts/cross_build.sh --preset rpi-release --install

# Build debug version
./scripts/cross_build.sh --preset rpi-debug --clean --install

# Build specific target only
./scripts/cross_build.sh --preset rpi-release --target hokuyo_hub

# Build with custom parallel jobs
./scripts/cross_build.sh --preset rpi-release --jobs 4 --verbose
```

**Script Options:**
- `--preset PRESET` - CMake preset to use (required)
- `--clean` - Clean build directory before building
- `--install` - Install after successful build
- `--target TARGET` - Build specific target only
- `--jobs N` - Number of parallel build jobs
- `--verbose` - Enable verbose output
- `--list-presets` - Show available presets

### Method 2: CMake Presets

Use CMake presets directly for more control:

**Available Presets:**
- `rpi-debug` - Debug build with symbols
- `rpi-release` - Optimized release build (recommended)
- `rpi-relwithdebinfo` - Release with debug symbols

```bash
# Configure for Raspberry Pi 5
cmake --preset rpi-release

# Build
cmake --build build/linux-arm64 --parallel

# Install to dist directory
cmake --build build/linux-arm64 --target install
```

### Method 3: Manual Cross-Compilation

For complete control over the build process:

```bash
# Create build directory
mkdir -p build/linux-arm64
cd build/linux-arm64

# Configure with toolchain
cmake \
  -DCMAKE_TOOLCHAIN_FILE=../../cmake/toolchains/linux-arm64.cmake \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=../../dist/linux-arm64 \
  -DHOKUYO_NNG_ENABLE=ON \
  -DUSE_OSC=ON \
  ../..

# Build
cmake --build . --parallel $(sysctl -n hw.ncpu)

# Install
cmake --build . --target install
```

## ⚙️ Cross-Compilation Configuration

### Toolchain Selection

The build system automatically detects available toolchains in this priority order:

1. **Environment Override:** `$CROSS_COMPILE_PREFIX`
2. **GNU Toolchain:** `aarch64-linux-gnu-*`
3. **musl Toolchain:** `aarch64-linux-musl-*`
4. **Generic:** `aarch64-unknown-linux-gnu-*`

### Sysroot Configuration

The system searches for sysroots in these locations:

1. **Environment:** `$TARGET_SYSROOT`
2. **Toolchain:** `$TOOLCHAIN_ROOT/aarch64-linux-gnu/sysroot`
3. **Homebrew GNU:** `/opt/homebrew/Cellar/aarch64-linux-gnu-gcc/*/toolchain/aarch64-linux-gnu/sysroot`
4. **Homebrew musl:** `/opt/homebrew/Cellar/musl-cross/*/libexec/aarch64-linux-musl`

**Setting Custom Sysroot:**
```bash
# Export for current session
export TARGET_SYSROOT="/path/to/rpi/sysroot"

# Or pass to CMake directly
cmake --preset rpi-release -DCMAKE_SYSROOT="/path/to/rpi/sysroot"
```

### Optimization Flags

Cross-compilation uses Raspberry Pi 5 specific optimizations:

- **CPU:** `-mcpu=cortex-a76 -mtune=cortex-a76`
- **Security:** `-fstack-protector-strong`
- **Position Independent:** `-fPIC`
- **Linking:** `-Wl,--as-needed` (GNU linker)

## 📁 Output Structure

Cross-compilation builds output to `dist/linux-arm64/`:

```
dist/linux-arm64/
├── hokuyo_hub                  # ARM64 Linux executable
├── config/                     # Configuration files
│   ├── default.yaml           # Default configuration
│   └── examples/              # Example configurations
├── webui/                     # Web interface files
│   ├── index.html
│   ├── styles.css
│   └── js/                    # JavaScript files
├── third_party/               # Dependencies (if bundled)
└── README.md                  # Platform-specific documentation
```

## 🚀 Deployment to Raspberry Pi 5

### Method 1: Direct Copy

```bash
# Copy entire dist directory
scp -r dist/linux-arm64/ pi@raspberrypi:~/hokuyo-hub/

# Or create archive and copy
tar -czf hokuyo-hub-rpi5.tar.gz -C dist linux-arm64
scp hokuyo-hub-rpi5.tar.gz pi@raspberrypi:~
ssh pi@raspberrypi "tar -xzf hokuyo-hub-rpi5.tar.gz"
```

### Method 2: Rsync (Incremental)

```bash
# Efficient incremental sync
rsync -avz --progress dist/linux-arm64/ pi@raspberrypi:~/hokuyo-hub/

# Exclude certain files
rsync -avz --progress --exclude='*.log' dist/linux-arm64/ pi@raspberrypi:~/hokuyo-hub/
```

### Method 3: Network File System

```bash
# Mount Raspberry Pi filesystem (if NFS is set up)
mount pi@raspberrypi:/ /mnt/rpi
cp -r dist/linux-arm64/* /mnt/rpi/opt/hokuyo-hub/
```

## 🔧 Raspberry Pi 5 Setup

### System Preparation

**1. Update System:**
```bash
sudo apt update && sudo apt upgrade -y
```

**2. Install Runtime Dependencies:**
```bash
# Essential dependencies
sudo apt install -y \
    libyaml-cpp0.7 \
    libnng1 \
    libssl3 \
    zlib1g \
    libjsoncpp25 \
    libbrotli1 \
    libuuid1

# Optional development tools
sudo apt install -y \
    curl \
    htop \
    screen \
    tmux
```

**3. Create Application Directory:**
```bash
mkdir -p ~/hokuyo-hub
cd ~/hokuyo-hub
```

### Network Configuration

**Configure Ethernet for Hokuyo Sensors:**
```bash
# Add static IP for sensor network
sudo nano /etc/dhcpcd.conf

# Add these lines:
# interface eth0
# static ip_address=192.168.1.10/24
# static routers=192.168.1.1
# static domain_name_servers=192.168.1.1

# Restart networking
sudo systemctl restart dhcpcd
```

**Alternative - Using netplan (Ubuntu):**
```yaml
# /etc/netplan/01-netcfg.yaml
network:
  version: 2
  ethernets:
    eth0:
      dhcp4: false
      addresses: [192.168.1.10/24]
      gateway4: 192.168.1.1
      nameservers:
        addresses: [8.8.8.8]
```

### Application Setup

**1. Make Executable:**
```bash
chmod +x hokuyo_hub
```

**2. Test Run:**
```bash
# Test with verbose output
./hokuyo_hub --config ./config/default.yaml --listen 0.0.0.0:8080 --verbose

# Check if it starts (Ctrl+C to stop)
```

**3. Configure Sensors:**

Edit `config/default.yaml` for your sensor setup:
```yaml
sensors:
  - id: "sensor1"
    name: "front-lidar"
    type: "hokuyo_urg_eth"
    endpoint: "192.168.1.100:10940"  # Your sensor IP
    pose: { tx: 0.0, ty: 0.0, theta: 0.0 }
    enabled: true
    mode: "ME"
    skip_step: 1
    interval: 0
    mask:
      angle: { min: -135.0, max: 135.0 }
      range: { near: 0.05, far: 15.0 }
```

## 🎛️ Service Configuration

### Systemd Service

Create a systemd service for automatic startup:

**1. Create Service File:**
```bash
sudo nano /etc/systemd/system/hokuyo-hub.service
```

**Service Configuration:**
```ini
[Unit]
Description=HokuyoHub LiDAR Processing Service
After=network.target
Wants=network-online.target

[Service]
Type=simple
User=pi
WorkingDirectory=/home/pi/hokuyo-hub
ExecStart=/home/pi/hokuyo-hub/hokuyo_hub --config ./config/default.yaml --listen 0.0.0.0:8080
Restart=always
RestartSec=3
StandardOutput=journal
StandardError=journal
Environment=HOME=/home/pi

[Install]
WantedBy=multi-user.target
```

**2. Enable and Start Service:**
```bash
# Enable service
sudo systemctl enable hokuyo-hub.service

# Start service
sudo systemctl start hokuyo-hub.service

# Check status
sudo systemctl status hokuyo-hub.service

# View logs
sudo journalctl -u hokuyo-hub.service -f
```

### Alternative - Screen Session

For development and testing:

```bash
# Start in screen session
screen -S hokuyo-hub
./hokuyo_hub --config ./config/default.yaml --listen 0.0.0.0:8080

# Detach: Ctrl+A, D
# Reattach: screen -r hokuyo-hub
```

## 📊 Performance Tuning

### Raspberry Pi 5 Optimizations

**1. CPU Governor:**
```bash
# Set performance mode
echo performance | sudo tee /sys/devices/system/cpu/cpu*/cpufreq/scaling_governor

# Or add to /etc/rc.local for persistence
```

**2. Memory Split:**
```bash
# Reduce GPU memory allocation
sudo nano /boot/firmware/config.txt
# Add: gpu_mem=64
sudo reboot
```

**3. USB/Ethernet Performance:**
```bash
# Increase network buffers
echo 'net.core.rmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
echo 'net.core.wmem_max = 16777216' | sudo tee -a /etc/sysctl.conf
sudo sysctl -p
```

### Application Tuning

**1. Sensor Configuration:**
```yaml
# Optimize for Pi 5 performance
sensors:
  - skip_step: 2          # Downsample data
    interval: 100         # 100ms between scans
    
# Reduce processing load
dbscan:
  h_min: 0.02            # Larger grid cells
  M_max: 400             # Fewer candidates
  
prefilter:
  neighborhood:
    r_base: 0.08         # Larger radius
```

**2. System Monitoring:**
```bash
# Monitor resource usage
htop
iotop
nethogs

# Check temperature
vcgencmd measure_temp
```

## 🔍 Troubleshooting

### Cross-Compilation Issues

**1. Toolchain Not Found:**
```bash
# Check available tools
which aarch64-linux-gnu-gcc
ls /opt/homebrew/bin/aarch64-*

# Reinstall toolchain
brew uninstall --ignore-dependencies aarch64-linux-gnu-gcc
brew install aarch64-linux-gnu-gcc
```

**2. CMake Configuration Fails:**
```bash
# Clear CMake cache
rm -rf build/linux-arm64/CMakeCache.txt

# Try different toolchain
export CROSS_COMPILE_PREFIX="aarch64-linux-musl-"
cmake --preset rpi-release
```

**3. Dependency Issues:**
```bash
# Force fetch mode to avoid cross-compilation dependency issues
cmake -DDEPS_MODE=fetch --preset rpi-release
```

### Deployment Issues

**1. Binary Won't Execute:**
```bash
# Check architecture on Pi
file hokuyo_hub
# Should show: ELF 64-bit LSB pie executable, ARM aarch64

# Check dynamic dependencies
ldd hokuyo_hub
# Install missing libraries with apt
```

**2. Permission Denied:**
```bash
# Fix permissions
chmod +x hokuyo_hub

# Check filesystem is not mounted noexec
mount | grep $(df . | tail -1 | awk '{print $1}')
```

**3. Missing Dependencies:**
```bash
# Install all dependencies
sudo apt install -y libyaml-cpp0.7 libnng1 libssl3 zlib1g \
                    libjsoncpp25 libbrotli1 libuuid1

# Check for missing libraries
ldd hokuyo_hub | grep "not found"
```

### Runtime Issues

**1. Network Configuration:**
```bash
# Test sensor connectivity
ping 192.168.1.100  # Your sensor IP
telnet 192.168.1.100 10940

# Check network interface
ip addr show
sudo ethtool eth0
```

**2. Service Issues:**
```bash
# Check service status
sudo systemctl status hokuyo-hub.service

# View detailed logs
sudo journalctl -u hokuyo-hub.service --since "1 hour ago"

# Restart service
sudo systemctl restart hokuyo-hub.service
```

**3. Performance Issues:**
```bash
# Monitor CPU usage
top -p $(pgrep hokuyo_hub)

# Check memory usage
cat /proc/$(pgrep hokuyo_hub)/status | grep Vm

# Monitor network traffic
sudo nethogs eth0
```

## 🧪 Testing and Validation

### Build Verification

```bash
# Verify build output
file dist/linux-arm64/hokuyo_hub
ls -la dist/linux-arm64/

# Test on Raspberry Pi
./hokuyo_hub --version
./hokuyo_hub --help
```

### Integration Testing

```bash
# Test REST API
curl http://raspberrypi.local:8080/api/health
curl http://raspberrypi.local:8080/api/sensors

# Test with real sensor
# Edit config with actual sensor IP and test
```

### Performance Benchmarking

```bash
# Monitor during operation
htop
iostat 1
sar -n DEV 1

# Log performance metrics
./hokuyo_hub --config ./config/default.yaml --listen 0.0.0.0:8080 2>&1 | \
grep -E "(fps|latency|cpu)" | tee performance.log
```

## 📚 Additional Resources

- **[Main Build Guide](../../BUILD.md)** - Overview of all build methods
- **[Quick Start Guide](../../QUICK_START.md)** - Get running quickly  
- **[Docker Build Guide](docker.md)** - Alternative containerized builds
- **[Troubleshooting Guide](troubleshooting.md)** - Common issues and solutions
- **[Advanced Configuration](advanced.md)** - Expert-level customization

---

**Raspberry Pi 5 Build Guide Version:** 1.0  
**Last Updated:** 2025-08-27  
**Target Platform:** Raspberry Pi 5 (ARM64 Linux)  
**Cross-Compilation:** macOS → linux-arm64