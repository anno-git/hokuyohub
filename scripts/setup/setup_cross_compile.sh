#!/bin/bash
# =============================================================================
# Cross-Compilation Environment Setup Script
# =============================================================================
# This script helps set up the cross-compilation environment for linux-arm64
# targeting Raspberry Pi 5.
#
# Usage:
#   ./scripts/setup_cross_compile.sh [--check-only]
#
# Options:
#   --check-only    Only check if tools are available, don't install anything

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
REQUIRED_TOOLS_GNU=(
    "aarch64-linux-gnu-gcc"
    "aarch64-linux-gnu-g++"
    "aarch64-linux-gnu-ar"
    "aarch64-linux-gnu-ranlib"
)

REQUIRED_TOOLS_MUSL=(
    "aarch64-linux-musl-gcc"
    "aarch64-linux-musl-g++"
    "aarch64-linux-musl-ar"
    "aarch64-linux-musl-ranlib"
)

HOMEBREW_PACKAGE_GNU="aarch64-linux-gnu-gcc"
HOMEBREW_PACKAGE_MUSL="musl-cross"
CHECK_ONLY=false

# Parse command line arguments
while [[ $# -gt 0 ]]; do
    case $1 in
        --check-only)
            CHECK_ONLY=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--check-only]"
            echo "  --check-only    Only check if tools are available"
            exit 0
            ;;
        *)
            echo "Unknown option: $1"
            exit 1
            ;;
    esac
done

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

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Function to check if Homebrew is available
check_homebrew() {
    if command_exists brew; then
        print_status "$GREEN" "✓ Homebrew is available"
        return 0
    else
        print_status "$RED" "✗ Homebrew not found"
        return 1
    fi
}

# Function to check cross-compilation tools
check_cross_tools() {
    local gnu_found=true
    local musl_found=true
    
    print_header "Checking Cross-Compilation Tools"
    
    # Check GNU toolchain
    print_status "$BLUE" "Checking GNU toolchain (aarch64-linux-gnu-*):"
    for tool in "${REQUIRED_TOOLS_GNU[@]}"; do
        if command_exists "$tool"; then
            local version
            version=$($tool --version | head -n1)
            print_status "$GREEN" "✓ $tool: $version"
        else
            print_status "$RED" "✗ $tool: Not found"
            gnu_found=false
        fi
    done
    
    # Check musl toolchain
    print_status "$BLUE" "Checking musl toolchain (aarch64-linux-musl-*):"
    for tool in "${REQUIRED_TOOLS_MUSL[@]}"; do
        if command_exists "$tool"; then
            local version
            version=$($tool --version | head -n1)
            print_status "$GREEN" "✓ $tool: $version"
        else
            print_status "$RED" "✗ $tool: Not found"
            musl_found=false
        fi
    done
    
    if [[ "$gnu_found" == true ]]; then
        print_status "$GREEN" "✓ GNU toolchain is available"
        return 0
    elif [[ "$musl_found" == true ]]; then
        print_status "$GREEN" "✓ musl toolchain is available"
        return 0
    else
        print_status "$RED" "✗ No compatible toolchain found"
        return 1
    fi
}

# Function to check for optional sysroot
check_sysroot() {
    print_header "Checking for Target Sysroot"
    
    if [[ -n "${TARGET_SYSROOT:-}" ]]; then
        if [[ -d "$TARGET_SYSROOT" ]]; then
            print_status "$GREEN" "✓ TARGET_SYSROOT set and exists: $TARGET_SYSROOT"
        else
            print_status "$YELLOW" "⚠ TARGET_SYSROOT set but directory doesn't exist: $TARGET_SYSROOT"
        fi
    else
        print_status "$YELLOW" "⚠ TARGET_SYSROOT not set (optional but recommended)"
        print_status "$YELLOW" "  Set with: export TARGET_SYSROOT=/path/to/rpi/sysroot"
    fi
    
    # Check for Homebrew installed sysroot
    if [[ -d "/opt/homebrew/Cellar/aarch64-linux-gnu-gcc" ]]; then
        local latest_version
        latest_version=$(ls -1 /opt/homebrew/Cellar/aarch64-linux-gnu-gcc/ | sort -V | tail -n1)
        local sysroot_path="/opt/homebrew/Cellar/aarch64-linux-gnu-gcc/$latest_version/toolchain/aarch64-linux-gnu/sysroot"
        
        if [[ -d "$sysroot_path" ]]; then
            print_status "$GREEN" "✓ Homebrew sysroot found: $sysroot_path"
        fi
    fi
}

# Function to install cross-compilation tools via Homebrew
install_cross_tools() {
    print_header "Installing Cross-Compilation Tools"
    
    if ! check_homebrew; then
        print_status "$RED" "Homebrew is required for automatic installation"
        print_status "$YELLOW" "Please install Homebrew first: https://brew.sh"
        return 1
    fi
    
    # Try to install musl-cross first (more commonly available)
    print_status "$BLUE" "Installing $HOMEBREW_PACKAGE_MUSL..."
    if brew install "$HOMEBREW_PACKAGE_MUSL"; then
        print_status "$GREEN" "✓ Successfully installed $HOMEBREW_PACKAGE_MUSL"
        return 0
    else
        print_status "$YELLOW" "⚠ Failed to install $HOMEBREW_PACKAGE_MUSL, trying GNU toolchain..."
    fi
    
    # Fallback to GNU toolchain
    print_status "$BLUE" "Installing $HOMEBREW_PACKAGE_GNU..."
    if brew install "$HOMEBREW_PACKAGE_GNU"; then
        print_status "$GREEN" "✓ Successfully installed $HOMEBREW_PACKAGE_GNU"
        return 0
    else
        print_status "$RED" "✗ Failed to install both toolchains"
        return 1
    fi
}

# Function to print environment setup instructions
print_environment_setup() {
    print_header "Environment Setup Instructions"
    
    echo "To use cross-compilation, you can optionally set these environment variables:"
    echo
    echo "# Optional: Override cross-compile prefix"
    echo "# For GNU toolchain:"
    echo "export CROSS_COMPILE_PREFIX=\"aarch64-linux-gnu-\""
    echo "# For musl toolchain:"
    echo "export CROSS_COMPILE_PREFIX=\"aarch64-linux-musl-\""
    echo
    echo "# Optional: Set target sysroot for better library detection"
    echo "export TARGET_SYSROOT=\"/path/to/raspberry-pi-sysroot\""
    echo
    echo "# Optional: Set toolchain root directory"
    echo "export TOOLCHAIN_ROOT=\"/opt/homebrew\""
    echo
    echo "Add these to your ~/.zshrc or ~/.bashrc to make them persistent."
}

# Function to print usage examples
print_usage_examples() {
    print_header "Cross-Compilation Usage Examples"
    
    echo "1. Configure for Raspberry Pi 5 Release build:"
    echo "   cmake --preset rpi-release"
    echo
    echo "2. Build the project:"
    echo "   cmake --build build/linux-arm64"
    echo
    echo "3. Or use the convenience script:"
    echo "   ./scripts/cross_build.sh --preset rpi-release"
    echo
    echo "4. Install to distribution directory:"
    echo "   cmake --build build/linux-arm64 --target install"
}

# Main execution
main() {
    print_header "Cross-Compilation Environment Setup"
    
    local tools_available=false
    
    # Check current state
    if check_cross_tools; then
        tools_available=true
        print_status "$GREEN" "✓ All required cross-compilation tools are available"
    else
        print_status "$YELLOW" "⚠ Some cross-compilation tools are missing"
    fi
    
    check_sysroot
    
    # Install tools if needed and not in check-only mode
    if [[ "$tools_available" == false && "$CHECK_ONLY" == false ]]; then
        echo
        read -p "Would you like to install missing tools via Homebrew? (y/N): " -n 1 -r
        echo
        if [[ $REPLY =~ ^[Yy]$ ]]; then
            if install_cross_tools; then
                print_status "$GREEN" "✓ Installation completed successfully"
                check_cross_tools
            else
                print_status "$RED" "✗ Installation failed"
                exit 1
            fi
        else
            print_status "$YELLOW" "Skipping installation. Manual setup required."
        fi
    fi
    
    print_environment_setup
    print_usage_examples
    
    print_header "Setup Complete"
    
    if [[ "$tools_available" == true || "$CHECK_ONLY" == false ]]; then
        print_status "$GREEN" "✓ Cross-compilation environment is ready!"
    else
        print_status "$YELLOW" "⚠ Cross-compilation tools need to be installed"
        exit 1
    fi
}

# Run main function
main "$@"