#!/bin/bash

# =============================================================================
# HokuyoHub CMake Presets Build Script - Phase 3 分離アーキテクチャ対応
# =============================================================================
# This script provides convenient access to CMake presets for building
# HokuyoHub with the separated architecture system.
#
# Usage: ./build_with_presets.sh [preset|service] [options]
#
# Presets:
#   debug       - Build macOS Debug configuration
#   release     - Build macOS Release configuration (default)
#   relwithdeb  - Build macOS RelWithDebInfo configuration
#   clean       - Clean build directories
#   list        - List available presets
#
# Service Commands:
#   start       - Start HokuyoHub separated system
#   stop        - Stop HokuyoHub separated system
#   restart     - Restart HokuyoHub separated system
#   status      - Check separated system status
#   dev         - Start in development mode
#
# Options:
#   --install   - Run install after build
#   --test      - Run tests after build
#   --package   - Create distribution package after build
#   --verbose   - Enable verbose output
#   --help      - Show this help message

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Logging functions
log_info() {
    echo -e "${BLUE}[INFO]${NC} $1"
}

log_success() {
    echo -e "${GREEN}[SUCCESS]${NC} $1"
}

log_warning() {
    echo -e "${YELLOW}[WARNING]${NC} $1"
}

log_error() {
    echo -e "${RED}[ERROR]${NC} $1"
}

# Show help
show_help() {
    cat << EOF
HokuyoHub CMake Presets Build Script - Phase 1

Usage: $0 [preset] [options]

Presets:
  debug       Build macOS Debug configuration
  release     Build macOS Release configuration (default)
  relwithdeb  Build macOS RelWithDebInfo configuration
  clean       Clean build directories
  list        List available presets

Options:
  --install   Run install after build
  --test      Run tests after build
  --package   Create package after build
  --verbose   Enable verbose output
  --help      Show this help message

Examples:
  $0                          # Build release (default)
  $0 debug --install          # Build debug and install
  $0 release --install --test # Build release, install, and test
  $0 clean                    # Clean build directories
  $0 list                     # List available presets

Phase 1 Features:
- Uses CMake presets for consistent builds
- Maintains backward compatibility with existing workflows
- Provides foundation for Phase 2 cross-compilation
- Includes comprehensive testing and packaging support

EOF
}

# Parse command line arguments
PRESET="release"
RUN_INSTALL=true
RUN_TEST=false
RUN_PACKAGE=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        debug|release|relwithdeb|clean|list|start|stop|restart|status|dev)
            PRESET="$1"
            shift
            ;;
        --install)
            RUN_INSTALL=true
            shift
            ;;
        --test)
            RUN_TEST=true
            shift
            ;;
        --package)
            RUN_PACKAGE=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            show_help
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
            echo "Use --help for usage information"
            exit 1
            ;;
    esac
done

# Verbose output helper
verbose_log() {
    if [[ "$VERBOSE" == "true" ]]; then
        echo "$1"
    fi
}

# List available presets
list_presets() {
    log_info "Available CMake presets:"
    echo
    echo "Configure presets:"
    cmake --list-presets 2>/dev/null || echo "  No configure presets available"
    echo
    echo "Build presets:"
    cmake --list-presets=build 2>/dev/null || echo "  No build presets available"
    echo
    echo "Test presets:"
    cmake --list-presets=test 2>/dev/null || echo "  No test presets available"
    echo
    echo "Package presets:"
    cmake --list-presets=package 2>/dev/null || echo "  No package presets available"
}

# Clean build directories
clean_build() {
    log_info "Cleaning build directories..."
    
    local dirs_to_clean=(
        "build/darwin-arm64"
        "dist/darwin-arm64"
        "third_party/urg_library"
    )
    
    for dir in "${dirs_to_clean[@]}"; do
        if [[ -d "$PROJECT_ROOT/$dir" ]]; then
            rm -rf "$PROJECT_ROOT/$dir"
            verbose_log "Removed $PROJECT_ROOT/$dir"
        fi
    done
    
    log_success "Build directories cleaned"
}

# Build with preset
build_with_preset() {
    local preset_name="$1"
    local configure_preset=""
    local build_preset=""
    
    case "$preset_name" in
        debug)
            configure_preset="mac-debug"
            build_preset="build-mac-debug"
            ;;
        release)
            configure_preset="mac-release"
            build_preset="build-mac-release"
            ;;
        relwithdeb)
            configure_preset="mac-relwithdebinfo"
            build_preset="build-mac-relwithdebinfo"
            ;;
        *)
            log_error "Unknown preset: $preset_name"
            exit 1
            ;;
    esac

    cd "$PROJECT_ROOT"
    
    # Configure
    log_info "Configuring with preset: $configure_preset"
    if [[ "$VERBOSE" == "true" ]]; then
        cmake --preset "$configure_preset"
    else
        cmake --preset "$configure_preset" >/dev/null 2>&1
    fi
    
    # Build
    log_info "Building with preset: $build_preset"
    if [[ "$VERBOSE" == "true" ]]; then
        cmake --build --preset "$build_preset"
    else
        cmake --build --preset "$build_preset" >/dev/null 2>&1
    fi
    
    log_success "Build completed successfully"
    
    # Install if requested
    if [[ "$RUN_INSTALL" == "true" ]]; then
        log_info "Installing..."
        if [[ "$VERBOSE" == "true" ]]; then
            cmake --install "build/darwin-arm64"
        else
            cmake --install "build/darwin-arm64" >/dev/null 2>&1
        fi
        log_success "Install completed"
    fi
    
    # Test if requested
    if [[ "$RUN_TEST" == "true" ]]; then
        log_info "Running tests..."
        local test_preset="test-mac-${preset_name}"
        if [[ "$VERBOSE" == "true" ]]; then
            ctest --preset "$test_preset" 2>/dev/null || log_warning "Tests not available or failed"
        else
            ctest --preset "$test_preset" >/dev/null 2>&1 || log_warning "Tests not available or failed"
        fi
    fi
    
    # Package if requested
    if [[ "$RUN_PACKAGE" == "true" ]]; then
        log_info "Creating distribution package..."
        
        # Create dist directory structure
        local dist_dir="$PROJECT_ROOT/dist/darwin-arm64"
        mkdir -p "$dist_dir"/{bin,webui-server,configs}
        
        # Copy backend binary
        if [[ -f "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub" ]]; then
            cp "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub" "$dist_dir/bin/"
            log_success "Backend binary packaged to: $dist_dir/bin/"
        else
            log_error "Backend binary not found for packaging"
            return 1
        fi
        
        # Copy WebUI server
        if [[ -d "$PROJECT_ROOT/webui-server" ]]; then
            cp -r "$PROJECT_ROOT/webui-server"/* "$dist_dir/webui-server/"
            log_success "WebUI server packaged to: $dist_dir/webui-server/"
        fi
        
        # Copy configs
        if [[ -d "$PROJECT_ROOT/configs" ]]; then
            cp -r "$PROJECT_ROOT/configs"/* "$dist_dir/configs/"
            log_success "Configs packaged to: $dist_dir/configs/"
        fi
        
        log_success "Distribution package created: $dist_dir/"
    fi
}

# Service management functions
detect_executable() {
    # Check preset build system paths (dist first, then build fallback)
    if [ -f "$PROJECT_ROOT/dist/darwin-arm64/bin/hokuyo_hub" ]; then
        echo "$PROJECT_ROOT/dist/darwin-arm64/bin/hokuyo_hub"
    elif [ -f "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub" ]; then
        echo "$PROJECT_ROOT/build/darwin-arm64/hokuyo_hub"
    elif [ -f "$PROJECT_ROOT/dist/linux-arm64/bin/hokuyo_hub" ]; then
        echo "$PROJECT_ROOT/dist/linux-arm64/bin/hokuyo_hub"
    elif [ -f "$PROJECT_ROOT/build/linux-arm64/hokuyo_hub" ]; then
        echo "$PROJECT_ROOT/build/linux-arm64/hokuyo_hub"
    else
        return 1
    fi
}

service_start() {
    local executable=$(detect_executable)
    if [ $? -ne 0 ] || [ ! -f "$executable" ]; then
        log_error "HokuyoHub executable not found. Please build first with: $0 release --install"
        exit 1
    fi
    
    log_info "Starting HokuyoHub service..."
    if [ -f "$PROJECT_ROOT/hokuyo_hub.pid" ]; then
        local pid=$(cat "$PROJECT_ROOT/hokuyo_hub.pid")
        if kill -0 "$pid" 2>/dev/null; then
            log_warning "HokuyoHub is already running (PID: $pid)"
            return 0
        else
            rm -f "$PROJECT_ROOT/hokuyo_hub.pid"
        fi
    fi
    
    cd "$PROJECT_ROOT"
    "$executable" --config configs/default.yaml &
    echo $! > hokuyo_hub.pid
    log_success "HokuyoHub started (PID: $(cat hokuyo_hub.pid))"
}

service_stop() {
    log_info "Stopping HokuyoHub service..."
    if [ -f "$PROJECT_ROOT/hokuyo_hub.pid" ]; then
        local pid=$(cat "$PROJECT_ROOT/hokuyo_hub.pid")
        if kill -0 "$pid" 2>/dev/null; then
            kill -TERM "$pid"
            
            # Wait for process to terminate (up to 10 seconds)
            local counter=0
            while kill -0 "$pid" 2>/dev/null && [ $counter -lt 50 ]; do
                sleep 0.2
                counter=$((counter + 1))
            done
            
            # If still running, force kill
            if kill -0 "$pid" 2>/dev/null; then
                log_warning "Process still running after TERM signal, sending KILL..."
                kill -KILL "$pid" 2>/dev/null || true
                sleep 1
            fi
            
            log_success "HokuyoHub stopped (PID: $pid)"
        else
            log_warning "Process not running (stale PID file)"
        fi
        rm -f "$PROJECT_ROOT/hokuyo_hub.pid"
    else
        log_warning "No PID file found, attempting to kill by name..."
        pkill -f "hokuyo_hub" && log_success "HokuyoHub stopped" || log_warning "No HokuyoHub process found"
        
        # Wait a moment to ensure any processes are fully terminated
        sleep 1
    fi
}

service_restart() {
    log_info "Restarting HokuyoHub service using external restart script..."
    
    # Check if we have the external restart script (from RestartManager)
    if [ -f "/tmp/hokuyo_restart.sh" ]; then
        log_info "Using existing restart script from RestartManager..."
        exec /tmp/hokuyo_restart.sh
    else
        # Fallback: simple restart
        log_info "No external restart script found, performing simple restart..."
        service_stop
        sleep 2
        service_start
    fi
}

service_status() {
    log_info "Checking HokuyoHub service status..."
    if [ -f "$PROJECT_ROOT/hokuyo_hub.pid" ]; then
        local pid=$(cat "$PROJECT_ROOT/hokuyo_hub.pid")
        if kill -0 "$pid" 2>/dev/null; then
            echo "✓ HokuyoHub is running (PID: $pid)"
        else
            echo "✗ HokuyoHub is not running (stale PID file)"
            rm -f "$PROJECT_ROOT/hokuyo_hub.pid"
        fi
    else
        if pgrep -f "hokuyo_hub" >/dev/null; then
            echo "✓ HokuyoHub process found (no PID file)"
            pgrep -f "hokuyo_hub"
        else
            echo "✗ HokuyoHub is not running"
        fi
    fi
}

service_dev() {
    local executable=$(detect_executable)
    if [ $? -ne 0 ] || [ ! -f "$executable" ]; then
        log_error "HokuyoHub executable not found. Please build first with: $0 release --install"
        exit 1
    fi
    
    log_info "Starting HokuyoHub in development mode..."
    cd "$PROJECT_ROOT"
    "$executable" --config configs/default.yaml
}

# Main execution
main() {
    echo "=== HokuyoHub CMake Presets Build Script - Phase 1 ==="
    echo "Project: $PROJECT_ROOT"
    echo "Command: $PRESET"
    echo
    
    case "$PRESET" in
        list)
            list_presets
            ;;
        clean)
            clean_build
            ;;
        debug|release|relwithdeb)
            build_with_preset "$PRESET"
            ;;
        start)
            service_start
            ;;
        stop)
            service_stop
            ;;
        restart)
            service_restart
            ;;
        status)
            service_status
            ;;
        dev)
            service_dev
            ;;
        *)
            log_error "Unknown command: $PRESET"
            show_help
            exit 1
            ;;
    esac
    
    echo
    log_success "Script completed successfully"
}

# Run main function
main "$@"