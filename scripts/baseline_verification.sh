#!/bin/bash

# =============================================================================
# HokuyoHub Baseline Build Verification Script
# =============================================================================
# This script establishes and verifies the baseline build state for Phase 0
# of the cross-compilation build system implementation.
#
# Usage: ./baseline_verification.sh [--capture-baseline] [--compare-baseline]
#
# Options:
#   --capture-baseline    Capture current build as new baseline
#   --compare-baseline    Compare current build against stored baseline
#   --clean              Clean build directories before building
#   --verbose            Enable verbose output
#
# Exit codes:
#   0: Success
#   1: Build failure
#   2: Baseline comparison failure
#   3: Missing dependencies

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PLATFORM="darwin-arm64"
BASELINE_DIR="$PROJECT_ROOT/build/$PLATFORM/baseline"
BUILD_DIR="$PROJECT_ROOT/build/$PLATFORM"
DIST_DIR="$PROJECT_ROOT/dist/$PLATFORM"

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

# Parse command line arguments
CAPTURE_BASELINE=false
COMPARE_BASELINE=false
CLEAN_BUILD=false
VERBOSE=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --capture-baseline)
            CAPTURE_BASELINE=true
            shift
            ;;
        --compare-baseline)
            COMPARE_BASELINE=true
            shift
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--capture-baseline] [--compare-baseline] [--clean] [--verbose]"
            exit 0
            ;;
        *)
            log_error "Unknown option: $1"
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

# Check system requirements
check_requirements() {
    log_info "Checking system requirements..."
    
    # Check if we're on macOS
    if [[ "$(uname)" != "Darwin" ]]; then
        log_error "This script is designed for macOS. Current system: $(uname)"
        exit 3
    fi
    
    # Check for required tools
    local missing_tools=()
    
    if ! command -v cmake >/dev/null 2>&1; then
        missing_tools+=("cmake")
    fi
    
    if ! command -v make >/dev/null 2>&1; then
        missing_tools+=("make")
    fi
    
    if [[ ${#missing_tools[@]} -gt 0 ]]; then
        log_error "Missing required tools: ${missing_tools[*]}"
        log_error "Please install missing tools and try again"
        exit 3
    fi
    
    # Check Homebrew prefix
    if [[ ! -d "/opt/homebrew" ]]; then
        log_warning "Homebrew not found at /opt/homebrew. Build may fail."
    fi
    
    log_success "System requirements check passed"
}

# Clean build directories
clean_build() {
    log_info "Cleaning build directories..."
    
    if [[ -d "$BUILD_DIR" ]]; then
        rm -rf "$BUILD_DIR"
        verbose_log "Removed $BUILD_DIR"
    fi
    
    if [[ -d "$DIST_DIR" ]]; then
        rm -rf "$DIST_DIR"
        verbose_log "Removed $DIST_DIR"
    fi
    
    # Clean third_party directory (urg_library build artifacts)
    if [[ -d "$PROJECT_ROOT/third_party/$PLATFORM" ]]; then
        rm -rf "$PROJECT_ROOT/third_party/$PLATFORM"
        verbose_log "Removed $PROJECT_ROOT/third_party/$PLATFORM"
    fi
    
    log_success "Build directories cleaned"
}

# Build the project
build_project() {
    log_info "Building HokuyoHub (macOS Release)..."
    
    cd "$PROJECT_ROOT"
    
    # Configure with CMake preset
    verbose_log "Configuring with cmake --preset mac-release"
    if [[ "$VERBOSE" == "true" ]]; then
        cmake --preset mac-release
    else
        cmake --preset mac-release >/dev/null 2>&1
    fi
    
    # Build the project
    verbose_log "Building with cmake --build build/$PLATFORM"
    if [[ "$VERBOSE" == "true" ]]; then
        cmake --build "build/$PLATFORM" --config Release
    else
        cmake --build "build/$PLATFORM" --config Release >/dev/null 2>&1
    fi
    
    # Install the project
    verbose_log "Installing with cmake --install build/$PLATFORM"
    if [[ "$VERBOSE" == "true" ]]; then
        cmake --install "build/$PLATFORM"
    else
        cmake --install "build/$PLATFORM" >/dev/null 2>&1
    fi
    
    log_success "Build completed successfully"
}

# Capture build artifacts and metrics
capture_artifacts() {
    local output_dir="$1"
    
    log_info "Capturing build artifacts to $output_dir..."
    
    mkdir -p "$output_dir"
    
    # Capture binary information
    local binary_path="$DIST_DIR/Release/hokuyo_hub"
    if [[ -f "$binary_path" ]]; then
        # Binary size
        stat -f%z "$binary_path" > "$output_dir/binary_size.txt"
        
        # Binary dependencies (otool -L)
        otool -L "$binary_path" > "$output_dir/dependencies.txt" 2>/dev/null || true
        
        # Binary architecture
        file "$binary_path" > "$output_dir/architecture.txt"
        
        # MD5 checksum
        md5 -q "$binary_path" > "$output_dir/checksum.txt"
        
        verbose_log "Binary size: $(cat "$output_dir/binary_size.txt") bytes"
    else
        log_error "Binary not found at $binary_path"
        return 1
    fi
    
    # Capture build configuration
    if [[ -f "$BUILD_DIR/Release/CMakeCache.txt" ]]; then
        cp "$BUILD_DIR/Release/CMakeCache.txt" "$output_dir/"
    elif [[ -f "$BUILD_DIR/CMakeCache.txt" ]]; then
        cp "$BUILD_DIR/CMakeCache.txt" "$output_dir/"
    fi
    
    # Capture installed files list
    find "$DIST_DIR" -type f | sort > "$output_dir/installed_files.txt"
    
    # Capture directory sizes
    du -sh "$BUILD_DIR" 2>/dev/null | cut -f1 > "$output_dir/build_dir_size.txt" || echo "N/A" > "$output_dir/build_dir_size.txt"
    du -sh "$DIST_DIR" 2>/dev/null | cut -f1 > "$output_dir/dist_dir_size.txt" || echo "N/A" > "$output_dir/dist_dir_size.txt"
    
    # Capture build timestamp
    date -Iseconds > "$output_dir/build_timestamp.txt"
    
    # Capture system information
    {
        echo "System: $(uname -a)"
        echo "CMake: $(cmake --version | head -1)"
        echo "Make: $(make --version | head -1)"
        echo "Xcode: $(xcode-select --print-path 2>/dev/null || echo "Not found")"
    } > "$output_dir/system_info.txt"
    
    log_success "Artifacts captured to $output_dir"
}

# Compare current build against baseline
compare_baseline() {
    log_info "Comparing current build against baseline..."
    
    if [[ ! -d "$BASELINE_DIR" ]]; then
        log_error "No baseline found. Run with --capture-baseline first."
        return 2
    fi
    
    local current_dir="$BASELINE_DIR/current"
    capture_artifacts "$current_dir"
    
    local baseline_dir="$BASELINE_DIR/baseline"
    local differences=()
    
    # Compare binary size
    if [[ -f "$baseline_dir/binary_size.txt" && -f "$current_dir/binary_size.txt" ]]; then
        local baseline_size=$(cat "$baseline_dir/binary_size.txt")
        local current_size=$(cat "$current_dir/binary_size.txt")
        
        if [[ "$baseline_size" != "$current_size" ]]; then
            local size_diff=$((current_size - baseline_size))
            differences+=("Binary size changed: $baseline_size -> $current_size bytes (${size_diff:+$size_diff})")
        fi
    fi
    
    # Compare dependencies
    if [[ -f "$baseline_dir/dependencies.txt" && -f "$current_dir/dependencies.txt" ]]; then
        if ! diff -q "$baseline_dir/dependencies.txt" "$current_dir/dependencies.txt" >/dev/null 2>&1; then
            differences+=("Binary dependencies changed")
        fi
    fi
    
    # Compare installed files
    if [[ -f "$baseline_dir/installed_files.txt" && -f "$current_dir/installed_files.txt" ]]; then
        if ! diff -q "$baseline_dir/installed_files.txt" "$current_dir/installed_files.txt" >/dev/null 2>&1; then
            differences+=("Installed files list changed")
        fi
    fi
    
    # Report results
    if [[ ${#differences[@]} -eq 0 ]]; then
        log_success "Build matches baseline - no regressions detected"
        return 0
    else
        log_warning "Build differences detected:"
        for diff in "${differences[@]}"; do
            echo "  - $diff"
        done
        
        # Show detailed diff if verbose
        if [[ "$VERBOSE" == "true" ]]; then
            log_info "Detailed differences:"
            
            if [[ -f "$baseline_dir/dependencies.txt" && -f "$current_dir/dependencies.txt" ]]; then
                echo "Dependencies diff:"
                diff "$baseline_dir/dependencies.txt" "$current_dir/dependencies.txt" || true
            fi
            
            if [[ -f "$baseline_dir/installed_files.txt" && -f "$current_dir/installed_files.txt" ]]; then
                echo "Installed files diff:"
                diff "$baseline_dir/installed_files.txt" "$current_dir/installed_files.txt" || true
            fi
        fi
        
        return 2
    fi
}

# Main execution
main() {
    echo "=== HokuyoHub Baseline Build Verification ==="
    echo "Project: $PROJECT_ROOT"
    echo "Baseline: $BASELINE_DIR"
    echo
    
    # Check requirements
    check_requirements
    
    # Clean if requested
    if [[ "$CLEAN_BUILD" == "true" ]]; then
        clean_build
    fi
    
    # Build the project
    build_project
    
    # Capture baseline if requested
    if [[ "$CAPTURE_BASELINE" == "true" ]]; then
        log_info "Capturing new baseline..."
        mkdir -p "$BASELINE_DIR"
        capture_artifacts "$BASELINE_DIR/baseline"
        log_success "Baseline captured successfully"
    fi
    
    # Compare against baseline if requested
    if [[ "$COMPARE_BASELINE" == "true" ]]; then
        compare_baseline
    fi
    
    # If neither capture nor compare was requested, just verify the build works
    if [[ "$CAPTURE_BASELINE" == "false" && "$COMPARE_BASELINE" == "false" ]]; then
        log_info "Build verification completed successfully"
        log_info "Binary location: $DIST_DIR/Release/hokuyo_hub"
        log_info "Use --capture-baseline to establish baseline"
        log_info "Use --compare-baseline to check for regressions"
    fi
    
    echo
    log_success "Baseline verification completed"
}

# Run main function
main "$@"