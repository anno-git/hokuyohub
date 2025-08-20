#!/bin/bash
# =============================================================================
# Cross-Compilation Build Script for Raspberry Pi 5
# =============================================================================
# This script provides a convenient interface for cross-compiling the Hokuyo
# sensor application for Raspberry Pi 5 (linux-arm64).
#
# Usage:
#   ./scripts/cross_build.sh [OPTIONS]
#
# Examples:
#   ./scripts/cross_build.sh --preset rpi-release
#   ./scripts/cross_build.sh --preset rpi-debug --clean
#   ./scripts/cross_build.sh --preset rpi-release --install
#   ./scripts/cross_build.sh --list-presets

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Default configuration
PRESET=""
CLEAN_BUILD=false
INSTALL_AFTER_BUILD=false
VERBOSE=false
JOBS=""
TARGET=""
LIST_PRESETS=false

# Available cross-compilation presets
CROSS_PRESETS=(
    "rpi-base"
    "rpi-debug"
    "rpi-release"
    "rpi-relwithdebinfo"
)

# Function to print colored output
print_status() {
    local color=$1
    local message=$2
    echo -e "${color}${message}${NC}"
}

print_header() {
    echo
    print_status "$BLUE" "=== $1 ==="
}

print_error() {
    print_status "$RED" "ERROR: $1"
}

print_warning() {
    print_status "$YELLOW" "WARNING: $1"
}

print_success() {
    print_status "$GREEN" "SUCCESS: $1"
}

# Function to show usage
show_usage() {
    cat << EOF
Cross-Compilation Build Script for Raspberry Pi 5

Usage: $0 [OPTIONS]

Options:
    --preset PRESET         CMake preset to use (required)
    --clean                 Clean build directory before building
    --install               Install after successful build
    --target TARGET         Specific target to build (default: all)
    --jobs N                Number of parallel jobs (default: auto)
    --verbose               Enable verbose output
    --list-presets          List available cross-compilation presets
    -h, --help              Show this help message

Available Cross-Compilation Presets:
$(printf "    %s\n" "${CROSS_PRESETS[@]}")

Examples:
    $0 --preset rpi-release
    $0 --preset rpi-debug --clean
    $0 --preset rpi-release --install
    $0 --preset rpi-release --target hokuyo_hub
    $0 --list-presets

Environment Variables:
    CROSS_COMPILE_PREFIX    Override cross-compile prefix (default: aarch64-linux-gnu-)
    TARGET_SYSROOT          Path to target sysroot (optional but recommended)
    TOOLCHAIN_ROOT          Path to toolchain installation (optional)

EOF
}

# Function to list available presets
list_presets() {
    print_header "Available Cross-Compilation Presets"
    
    echo "Configure Presets:"
    for preset in "${CROSS_PRESETS[@]}"; do
        echo "  - $preset"
    done
    
    echo
    echo "Build Presets:"
    echo "  - build-rpi-debug"
    echo "  - build-rpi-release"
    echo "  - build-rpi-relwithdebinfo"
    echo "  - build-all-rpi"
    
    echo
    echo "Use 'cmake --list-presets' for complete preset information."
}

# Function to validate preset
validate_preset() {
    local preset=$1
    for valid_preset in "${CROSS_PRESETS[@]}"; do
        if [[ "$preset" == "$valid_preset" ]]; then
            return 0
        fi
    done
    return 1
}

# Function to check if cross-compilation tools are available
check_cross_tools() {
    local required_tools=(
        "aarch64-linux-gnu-gcc"
        "aarch64-linux-gnu-g++"
    )
    
    for tool in "${required_tools[@]}"; do
        if ! command -v "$tool" >/dev/null 2>&1; then
            print_error "Cross-compilation tool not found: $tool"
            echo "Run './scripts/setup_cross_compile.sh' to set up the environment."
            return 1
        fi
    done
    
    return 0
}

# Function to clean build directory
clean_build_dir() {
    local build_dir="build/linux-arm64"
    
    if [[ -d "$build_dir" ]]; then
        print_status "$YELLOW" "Cleaning build directory: $build_dir"
        rm -rf "$build_dir"
        print_success "Build directory cleaned"
    else
        print_status "$BLUE" "Build directory doesn't exist, no cleaning needed"
    fi
}

# Function to configure the project
configure_project() {
    local preset=$1
    
    print_header "Configuring Project with Preset: $preset"
    
    local cmake_args=("--preset" "$preset")
    
    if [[ "$VERBOSE" == true ]]; then
        cmake_args+=("--verbose")
    fi
    
    print_status "$BLUE" "Running: cmake ${cmake_args[*]}"
    
    if cmake "${cmake_args[@]}"; then
        print_success "Configuration completed successfully"
        return 0
    else
        print_error "Configuration failed"
        return 1
    fi
}

# Function to build the project
build_project() {
    local preset=$1
    local build_dir="build/linux-arm64"
    
    print_header "Building Project"
    
    if [[ ! -d "$build_dir" ]]; then
        print_error "Build directory doesn't exist. Run configuration first."
        return 1
    fi
    
    local cmake_args=("--build" "$build_dir")
    
    if [[ -n "$TARGET" ]]; then
        cmake_args+=("--target" "$TARGET")
    fi
    
    if [[ -n "$JOBS" ]]; then
        cmake_args+=("--parallel" "$JOBS")
    fi
    
    if [[ "$VERBOSE" == true ]]; then
        cmake_args+=("--verbose")
    fi
    
    print_status "$BLUE" "Running: cmake ${cmake_args[*]}"
    
    if cmake "${cmake_args[@]}"; then
        print_success "Build completed successfully"
        return 0
    else
        print_error "Build failed"
        return 1
    fi
}

# Function to install the project
install_project() {
    local build_dir="build/linux-arm64"
    
    print_header "Installing Project"
    
    local cmake_args=("--build" "$build_dir" "--target" "install")
    
    if [[ "$VERBOSE" == true ]]; then
        cmake_args+=("--verbose")
    fi
    
    print_status "$BLUE" "Running: cmake ${cmake_args[*]}"
    
    if cmake "${cmake_args[@]}"; then
        print_success "Installation completed successfully"
        print_status "$GREEN" "Binaries installed to: dist/linux-arm64/"
        return 0
    else
        print_error "Installation failed"
        return 1
    fi
}

# Function to print build summary
print_build_summary() {
    local preset=$1
    local build_dir="build/linux-arm64"
    local dist_dir="dist/linux-arm64"
    
    print_header "Build Summary"
    
    echo "Preset used: $preset"
    echo "Build directory: $build_dir"
    echo "Install directory: $dist_dir"
    
    if [[ -d "$build_dir" ]]; then
        echo "Build artifacts:"
        find "$build_dir" -name "hokuyo_hub" -type f 2>/dev/null | head -5 | while read -r file; do
            echo "  - $file"
        done
    fi
    
    if [[ -d "$dist_dir" && "$INSTALL_AFTER_BUILD" == true ]]; then
        echo "Installed files:"
        find "$dist_dir" -type f 2>/dev/null | head -10 | while read -r file; do
            echo "  - $file"
        done
    fi
}

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --preset)
            PRESET="$2"
            shift 2
            ;;
        --clean)
            CLEAN_BUILD=true
            shift
            ;;
        --install)
            INSTALL_AFTER_BUILD=true
            shift
            ;;
        --target)
            TARGET="$2"
            shift 2
            ;;
        --jobs)
            JOBS="$2"
            shift 2
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        --list-presets)
            LIST_PRESETS=true
            shift
            ;;
        -h|--help)
            show_usage
            exit 0
            ;;
        *)
            print_error "Unknown option: $1"
            show_usage
            exit 1
            ;;
    esac
done

# Main execution
main() {
    print_header "Cross-Compilation Build for Raspberry Pi 5"
    
    # Handle list presets
    if [[ "$LIST_PRESETS" == true ]]; then
        list_presets
        exit 0
    fi
    
    # Validate required arguments
    if [[ -z "$PRESET" ]]; then
        print_error "Preset is required. Use --preset option."
        show_usage
        exit 1
    fi
    
    # Validate preset
    if ! validate_preset "$PRESET"; then
        print_error "Invalid preset: $PRESET"
        echo "Available presets: ${CROSS_PRESETS[*]}"
        exit 1
    fi
    
    # Check cross-compilation tools
    if ! check_cross_tools; then
        exit 1
    fi
    
    # Print environment info
    print_status "$BLUE" "Cross-compilation environment:"
    echo "  Preset: $PRESET"
    echo "  Clean build: $CLEAN_BUILD"
    echo "  Install after build: $INSTALL_AFTER_BUILD"
    [[ -n "$TARGET" ]] && echo "  Target: $TARGET"
    [[ -n "$JOBS" ]] && echo "  Parallel jobs: $JOBS"
    [[ -n "${CROSS_COMPILE_PREFIX:-}" ]] && echo "  Cross-compile prefix: $CROSS_COMPILE_PREFIX"
    [[ -n "${TARGET_SYSROOT:-}" ]] && echo "  Target sysroot: $TARGET_SYSROOT"
    
    # Execute build steps
    if [[ "$CLEAN_BUILD" == true ]]; then
        clean_build_dir
    fi
    
    if ! configure_project "$PRESET"; then
        exit 1
    fi
    
    if ! build_project "$PRESET"; then
        exit 1
    fi
    
    if [[ "$INSTALL_AFTER_BUILD" == true ]]; then
        if ! install_project; then
            exit 1
        fi
    fi
    
    print_build_summary "$PRESET"
    
    print_header "Cross-Compilation Complete"
    print_success "All operations completed successfully!"
    
    if [[ "$INSTALL_AFTER_BUILD" == false ]]; then
        echo
        print_status "$YELLOW" "Tip: Use --install to automatically install after building"
    fi
}

# Run main function
main "$@"