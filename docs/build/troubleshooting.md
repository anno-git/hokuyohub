# Build Troubleshooting Guide

**Common issues and solutions for building HokuyoHub across all platforms**

This guide provides solutions to frequently encountered build issues, organized by platform and problem category. For platform-specific build instructions, see the [main BUILD.md](../../BUILD.md).

## 🌟 CrowCpp Migration Benefits

HokuyoHub now uses **CrowCpp** instead of the previous web framework, eliminating many common build issues:

- **⚡ No Framework Compilation**: Header-only design eliminates web framework build errors
- **🔧 Simplified Dependencies**: Fewer external libraries to troubleshoot
- **🚀 Faster Builds**: Reduced compilation time and complexity
- **📈 Better Error Messages**: Cleaner build output and easier debugging

## � Quick Diagnosis

### Build Environment Check

**Verify System Prerequisites:**
```bash
# Check CMake version (must be 3.18+)
cmake --version

# Check compiler availability
which gcc g++ || which clang clang++

# Check platform architecture
uname -m  # Should show arm64 on Apple Silicon, x86_64 on Intel

# Check available disk space (need 5GB+ for builds)
df -h .
```

**Common Quick Fixes:**
```bash
# Clean and retry
./scripts/build_with_presets.sh clean
./scripts/build_with_presets.sh release --install

# Force dependency fetch mode
cmake -DDEPS_MODE=fetch --preset mac-release

# Try different preset
cmake --preset mac-debug  # Instead of release
```

## 🍎 macOS Specific Issues

### CMake and Build Tools

**Issue: CMake Version Too Old**
```
CMake 3.18 or higher is required. You are running version X.Y.Z
```
**Solution:**
```bash
# Update CMake via Homebrew
brew upgrade cmake

# Or install if not present
brew install cmake

# Verify version
cmake --version
```

**Issue: No CMAKE_C_COMPILER Found**
```
No CMAKE_C_COMPILER could be found
```
**Solution:**
```bash
# Install Xcode Command Line Tools
xcode-select --install

# If already installed, reset tools
sudo xcode-select --reset
sudo xcode-select --install

# Verify installation
xcode-select -p  # Should show path to tools
```

**Issue: Homebrew Permissions**
```
Error: Permission denied @ dir_s_mkdir - /opt/homebrew
```
**Solution:**
```bash
# Fix Homebrew ownership
sudo chown -R $(whoami) /opt/homebrew

# Or use alternative Homebrew location
export HOMEBREW_PREFIX=/usr/local
```

### Dependency Issues

**Issue: yaml-cpp Not Found**
```
Could not find yaml-cpp (missing: yaml-cpp_DIR)
```
**Solutions:**
```bash
# Option 1: Install via Homebrew
brew install yaml-cpp

# Option 2: Set CMAKE_PREFIX_PATH
export CMAKE_PREFIX_PATH=/opt/homebrew:$CMAKE_PREFIX_PATH
cmake --preset mac-release

# Option 3: Use fetch mode
cmake -DDEPS_MODE=fetch --preset mac-release
```

**Issue: Web Framework Build Issues (Legacy)**
```
Note: HokuyoHub now uses CrowCpp (header-only) instead of the previous web framework
```
**Solutions:**
```bash
# CrowCpp is header-only - no compilation needed
# If you encounter web framework errors, ensure you're using latest version

# For legacy the previous web framework builds, clean and rebuild:
./scripts/build_with_presets.sh clean
cmake --preset mac-release

# CrowCpp benefits: No external web framework dependencies to install
```

**Issue: NNG Library Issues**
```
Could not find NNG library
```
**Solutions:**
```bash
# Install via Homebrew
brew install nng

# Or disable NNG support
cmake -DHOKUYO_NNG_ENABLE=OFF --preset mac-release

# Or build from source
cmake -DDEPS_NNG=fetch --preset mac-release
```

### Compilation Errors

**Issue: C++20 Standard Not Available**
```
C++20 features are not supported
```
**Solutions:**
```bash
# Update Xcode Command Line Tools
sudo xcode-select --install

# Or use older C++ standard (not recommended)
cmake -DCMAKE_CXX_STANDARD=17 --preset mac-release

# Check compiler version
clang++ --version  # Should be 12.0+
```

**Issue: Apple Silicon vs Intel Compatibility**
```
Architecture mismatch errors
```
**Solutions:**
```bash
# For Apple Silicon, ensure Homebrew is ARM64
/opt/homebrew/bin/brew --version

# For Intel Macs, use x86_64 Homebrew
/usr/local/bin/brew --version

# Force architecture if needed
arch -arm64 cmake --preset mac-release  # Apple Silicon
arch -x86_64 cmake --preset mac-release  # Intel
```

### Runtime Issues

**Issue: Binary Won't Execute**
```
./hokuyo_hub: Bad CPU type in executable
```
**Solutions:**
```bash
# Check binary architecture
file dist/darwin-arm64/hokuyo_hub

# Rebuild for correct architecture
./scripts/build_with_presets.sh clean
cmake --preset mac-release
cmake --build build/darwin-arm64
```

**Issue: Dynamic Library Loading**
```
dyld: Library not loaded: @rpath/...
```
**Solutions:**
```bash
# Check library dependencies
otool -L dist/darwin-arm64/hokuyo_hub

# Fix library paths
install_name_tool -change /old/path /opt/homebrew/lib/libname.dylib dist/darwin-arm64/hokuyo_hub

# Or reinstall with proper RPATH
cmake -DCMAKE_INSTALL_RPATH=/opt/homebrew/lib --preset mac-release
```

## 🥧 Cross-Compilation Issues (Raspberry Pi 5)

### Toolchain Setup

**Issue: Cross-Compilation Tools Not Found**
```
aarch64-linux-gnu-gcc: command not found
```
**Solutions:**
```bash
# Run setup script
./scripts/setup_cross_compile.sh

# Manual installation
brew install aarch64-linux-gnu-gcc

# Alternative toolchain
brew install musl-cross

# Verify installation
which aarch64-linux-gnu-gcc
aarch64-linux-gnu-gcc --version
```

**Issue: Toolchain Version Conflicts**
```
Different versions of cross-compilation tools
```
**Solutions:**
```bash
# Clean reinstall
brew uninstall --ignore-dependencies aarch64-linux-gnu-gcc
brew install aarch64-linux-gnu-gcc

# Use specific toolchain prefix
export CROSS_COMPILE_PREFIX="aarch64-linux-musl-"
./scripts/cross_build.sh --preset rpi-release
```

**Issue: Sysroot Not Found**
```
Warning: No sysroot found. Library detection may be limited.
```
**Solutions:**
```bash
# Set target sysroot (optional but recommended)
export TARGET_SYSROOT="/path/to/raspberry-pi-sysroot"

# Or continue without sysroot (may have limited functionality)
cmake --preset rpi-release  # Will use bundled/fetch dependencies
```

### Cross-Build Configuration

**Issue: CMake Can't Find Cross Tools**
```
The CMAKE_C_COMPILER: aarch64-linux-gnu-gcc is not able to compile
```
**Solutions:**
```bash
# Check PATH includes toolchain
echo $PATH | grep homebrew

# Add to PATH if missing
export PATH="/opt/homebrew/bin:$PATH"

# Specify toolchain root
export TOOLCHAIN_ROOT="/opt/homebrew"
cmake --preset rpi-release
```

**Issue: External Project Build Fails**
```
Error building URG library for ARM64
```
**Solutions:**
```bash
# Check cross-compilation environment
./scripts/cross_build.sh --preset rpi-debug --verbose

# Force rebuild URG library
rm -rf build/linux-arm64/_deps/urg*
cmake --build build/linux-arm64

# Use bundled version
cmake -DDEPS_URG=bundled --preset rpi-release
```

### Target Platform Issues

**Issue: Wrong Architecture Generated**
```
file shows x86_64 instead of ARM aarch64
```
**Solutions:**
```bash
# Verify toolchain is cross-compiler
aarch64-linux-gnu-gcc -dumpmachine  # Should show aarch64-linux-gnu

# Clean and rebuild
./scripts/cross_build.sh --preset rpi-release --clean

# Check CMake cache for wrong compiler
grep -i compiler build/linux-arm64/CMakeCache.txt
```

**Issue: Raspberry Pi Runtime Errors**
```
./hokuyo_hub: No such file or directory (on Pi)
```
**Solutions:**
```bash
# Check if binary is ARM64
file hokuyo_hub  # On Raspberry Pi

# Install missing runtime dependencies
sudo apt update
sudo apt install -y libyaml-cpp0.7 libnng1 libssl3 zlib1g

# Check dynamic library dependencies
ldd hokuyo_hub  # On Raspberry Pi
```

## 🐳 Docker Build Issues

### Docker Environment

**Issue: Docker BuildKit Not Available**
```
buildx: command not found
```
**Solutions:**
```bash
# Update Docker Desktop to latest version
# Or install buildx plugin manually

# Enable BuildKit
export DOCKER_BUILDKIT=1

# Create and use buildx builder
docker buildx create --name hokuyo-builder --use
```

**Issue: Multi-Platform Not Supported**
```
multiple platforms feature is not supported
```
**Solutions:**
```bash
# Install QEMU for emulation
docker run --privileged --rm tonistiigi/binfmt --install arm64

# Verify platform support
docker buildx inspect --bootstrap

# Create multi-platform builder
docker buildx create --name multiplatform --driver docker-container --use
```

**Issue: Out of Memory During Build**
```
Build process killed (exit code 137)
```
**Solutions:**
```bash
# Increase Docker memory limit (Docker Desktop: Settings > Resources)
# Or build with limited parallelism
docker buildx build --build-arg JOBS=2 --platform linux/arm64

# Clean up Docker to free space
docker system prune -af
docker builder prune -af
```

### Build Process Issues

**Issue: Platform Mismatch Errors**
```
exec /usr/bin/bash: exec format error
```
**Solutions:**
```bash
# Explicitly specify target platform
docker buildx build --platform linux/arm64 --load

# Check builder platforms
docker buildx inspect

# Use platform-specific base image
FROM --platform=$TARGETPLATFORM debian:bookworm-slim
```

**Issue: Dependency Resolution in Container**
```
Package not found during apt-get install
```
**Solutions:**
```bash
# Update package lists in Dockerfile
RUN apt-get update && apt-get install -y package-name

# Use specific package versions
RUN apt-get update && apt-get install -y package-name=version

# Check available packages
docker run --rm debian:bookworm-slim apt-cache search package-name
```

**Issue: Cross-Compilation in Container**
```
Cannot cross-compile dependencies in Docker
```
**Solutions:**
```bash
# Use native ARM64 base image
FROM --platform=linux/arm64 debian:bookworm-slim

# Or set up cross-compilation in container
RUN apt-get install -y gcc-aarch64-linux-gnu
ENV CC=aarch64-linux-gnu-gcc CXX=aarch64-linux-gnu-g++
```

### Runtime and Extraction Issues

**Issue: Cannot Extract Artifacts**
```
Error: Docker image 'hokuyo-hub:latest' not found
```
**Solutions:**
```bash
# Check available images
docker images | grep hokuyo

# Build image first
./docker/build.sh build-all

# Use correct image name
./scripts/extract_docker_artifacts.sh hokuyo-hub:runtime-arm64
```

**Issue: Extracted Binary Won't Run**
```
exec format error when running extracted binary
```
**Solutions:**
```bash
# Verify binary architecture
file dist/linux-arm64/hokuyo_hub

# Check if running on compatible platform
uname -m  # Should be aarch64 on target system

# Use Docker container instead
docker run -p 8080:8080 hokuyo-hub:latest
```

## 🔧 General Build Issues

### Environment and Dependencies

**Issue: Git Submodule Problems**
```
fatal: not a git repository (or any parent directories)
```
**Solutions:**
```bash
# Initialize and update submodules
git submodule update --init --recursive

# Or clone with submodules
git clone --recursive <repository-url>

# Reset submodules if corrupted
git submodule foreach --recursive git clean -fd
git submodule update --init --recursive
```

**Issue: Network/Firewall Issues**
```
Failed to download dependencies from GitHub/external sources
```
**Solutions:**
```bash
# Use system dependencies instead
cmake -DDEPS_MODE=system --preset mac-release

# Configure proxy if needed
export HTTP_PROXY=http://proxy:port
export HTTPS_PROXY=http://proxy:port

# Use alternative package sources
cmake -DDEPS_MODE=bundled --preset mac-release
```

**Issue: Insufficient Disk Space**
```
No space left on device
```
**Solutions:**
```bash
# Check available space
df -h .

# Clean build artifacts
./scripts/build_with_presets.sh clean
rm -rf build/ dist/

# Clean package manager caches
brew cleanup  # macOS
docker system prune -af  # Docker
```

### Configuration Issues

**Issue: Invalid CMake Preset**
```
CMake Error: No such preset in CMakePresets.json: "invalid-preset"
```
**Solutions:**
```bash
# List available presets
cmake --list-presets

# Use correct preset name
./scripts/cross_build.sh --list-presets

# Check CMakePresets.json syntax
cmake --preset mac-release  # Test basic preset
```

**Issue: Feature Toggle Conflicts**
```
Configuration conflicts with enabled features
```
**Solutions:**
```bash
# Use compatible feature combinations
cmake -DHOKUYO_NNG_ENABLE=ON -DUSE_OSC=ON --preset mac-release

# Disable conflicting features
cmake -DHOKUYO_NNG_ENABLE=OFF --preset mac-release

# Check feature dependencies in CMakeLists.txt
```

### Runtime Issues

**Issue: Port Already in Use**
```
Address already in use: bind() failed
```
**Solutions:**
```bash
# Find process using port
lsof -i :8080
netstat -an | grep 8080

# Kill process if needed
kill -9 $(lsof -t -i:8080)

# Use different port
./hokuyo_hub --listen 0.0.0.0:8081
```

**Issue: Configuration File Not Found**
```
Failed to load configuration file: default.yaml
```
**Solutions:**
```bash
# Check file exists and path is correct
ls -la configs/default.yaml

# Use absolute path
./hokuyo_hub --config $(pwd)/configs/default.yaml

# Copy from examples if missing
cp configs/examples/basic.yaml configs/default.yaml
```

**Issue: Sensor Connection Failures**
```
Failed to connect to sensor at IP:port
```
**Solutions:**
```bash
# Test network connectivity
ping sensor-ip
telnet sensor-ip sensor-port

# Check firewall settings
sudo ufw status  # Linux
pfctl -s all  # macOS

# Verify sensor configuration
# Check IP, port, and sensor type in config file
```

## 📊 Performance Issues

### Build Performance

**Issue: Very Slow Compilation**
```
Build takes hours instead of minutes (mostly resolved with CrowCpp migration)
```
**Solutions:**
```bash
# Use parallel builds
cmake --build build/darwin-arm64 --parallel $(sysctl -n hw.ncpu)

# Install ccache for faster rebuilds
brew install ccache
cmake -DCMAKE_CXX_COMPILER_LAUNCHER=ccache --preset mac-release

# Use faster build system
brew install ninja
cmake -G Ninja --preset mac-release

# Note: CrowCpp header-only design significantly reduces build times
```

**Issue: Memory Usage During Build**
```
System becomes unresponsive during build
```
**Solutions:**
```bash
# Reduce parallel jobs
cmake --build build/darwin-arm64 --parallel 2

# Monitor memory usage
top -o MEM
activity monitor  # macOS GUI

# Close other applications during build
```

### Runtime Performance

**Issue: High CPU Usage**
```
Application uses 100% CPU constantly
```
**Solutions:**
```yaml
# Optimize sensor configuration
sensors:
  - skip_step: 2      # Downsample data
    interval: 100     # Increase scan interval

# Reduce processing complexity
dbscan:
  h_min: 0.02        # Larger grid size
  M_max: 400         # Fewer candidates
```

**Issue: Memory Leaks**
```
Memory usage grows over time
```
**Solutions:**
```bash
# Monitor with instruments (macOS)
instruments -t "Leaks" ./hokuyo_hub

# Use valgrind (Linux)
valgrind --leak-check=full ./hokuyo_hub

# Enable debug build for better diagnostics
cmake --preset mac-debug
```

## 🛠️ Debugging Techniques

### Build Debugging

**Verbose Build Output:**
```bash
# CMake verbose configuration
cmake --preset mac-release --debug-output

# Verbose compilation
cmake --build build/darwin-arm64 --verbose

# Make with verbose output
make VERBOSE=1 -C build/darwin-arm64
```

**Detailed Error Analysis:**
```bash
# Save full build log
./scripts/build_with_presets.sh release --verbose 2>&1 | tee build.log

# Check specific component logs
cat build/darwin-arm64/CMakeFiles/CMakeError.log
cat build/darwin-arm64/CMakeFiles/CMakeOutput.log
```

### Runtime Debugging

**Debug Build and Symbols:**
```bash
# Build debug version
cmake --preset mac-debug
cmake --build build/darwin-arm64
cmake --install build/darwin-arm64

# Run with debugger
cd dist/darwin-arm64
lldb ./hokuyo_hub
```

**Log Analysis:**
```bash
# Enable verbose logging
./hokuyo_hub --config ./configs/default.yaml --verbose 2>&1 | tee app.log

# Monitor system calls (macOS)
sudo dtruss -p $(pgrep hokuyo_hub)

# Monitor file access
sudo fs_usage -w -f pathname $(pgrep hokuyo_hub)
```

## 📞 Getting Help

### Information to Collect

When reporting issues, include:

**System Information:**
```bash
# Operating system and version
sw_vers  # macOS
uname -a  # Linux

# Architecture
uname -m

# CMake version
cmake --version

# Compiler information
gcc --version || clang --version

# Homebrew/package manager info
brew --version  # macOS
```

**Build Information:**
```bash
# CMake configuration
cmake --preset mac-release --debug-output 2>&1 | head -50

# Build environment
env | grep -E "(CMAKE|CC|CXX|PATH)"

# Available presets
cmake --list-presets
```

**Error Details:**
- Complete error message and stack trace
- Build logs with verbose output
- Configuration files used
- Steps to reproduce the issue

### Community Resources

- **GitHub Issues** - Report bugs and feature requests
- **Build Documentation** - Check [BUILD.md](../../BUILD.md) for official procedures
- **Platform-Specific Guides** - See [macOS](macos.md), [Raspberry Pi](raspberry-pi.md), [Docker](docker.md)
- **Advanced Configuration** - Check [advanced.md](advanced.md) for expert options

---

**Troubleshooting Guide Version:** 1.0  
**Last Updated:** 2025-08-27  
**Covers:** macOS, Cross-compilation, Docker, Runtime issues  
**Build System:** CMake 3.18+ with presets