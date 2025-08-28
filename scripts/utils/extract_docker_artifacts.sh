#!/bin/bash
# =============================================================================
# Docker Artifact Extraction Script for Hokuyo Hub
# =============================================================================
# This script extracts built artifacts from a Docker container to the dist folder
# for final validation and deployment.
#
# Usage:
#   ./scripts/extract_docker_artifacts.sh [image_name]
#
# Example:
#   ./scripts/extract_docker_artifacts.sh hokuyo-hub:build-rebuild

set -e

# Configuration
IMAGE_NAME="${1:-hokuyo-hub:build-rebuild}"
PLATFORM_PAIR="${2:-linux-arm64}"
DIST_DIR="./dist/${PLATFORM_PAIR}"
TEMP_CONTAINER="hokuyo-extract-temp"

# Extract platform from platform-pair (e.g., linux-amd64 -> linux/amd64)
PLATFORM="${PLATFORM_PAIR//-//}"

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

# Cleanup function
cleanup() {
    log_info "Cleaning up temporary container..."
    docker rm -f "$TEMP_CONTAINER" >/dev/null 2>&1 || true
}

# Set trap for cleanup
trap cleanup EXIT

# Main extraction function
extract_artifacts() {
    log_info "Starting Docker artifact extraction..."
    log_info "Image: $IMAGE_NAME"
    log_info "Destination: $DIST_DIR"
    log_info "Platform-specific directory structure: dist/linux-arm64/"
    
    # Check if image exists
    if ! docker image inspect "$IMAGE_NAME" >/dev/null 2>&1; then
        log_error "Docker image '$IMAGE_NAME' not found"
        log_info "Available images:"
        docker images | grep hokuyo || echo "No hokuyo images found"
        exit 1
    fi
    
    # Create dist directory structure
    log_info "Creating platform-specific dist directory structure..."
    rm -rf "$DIST_DIR"
    mkdir -p "$DIST_DIR"
    
    # Create temporary container
    log_info "Creating temporary container..."
    docker create --name "$TEMP_CONTAINER" --platform "$PLATFORM" "$IMAGE_NAME" >/dev/null
    
    # Extract main binary
    log_info "Extracting main binary..."
    if docker cp "$TEMP_CONTAINER:/opt/hokuyohub/hokuyo_hub" "$DIST_DIR/" 2>/dev/null; then
        log_success "Main binary extracted successfully"
    else
        log_warning "Failed to extract main binary from runtime directory, trying build directory..."
        if docker cp "$TEMP_CONTAINER:/build/build/linux-arm64/hokuyo_hub" "$DIST_DIR/" 2>/dev/null; then
            log_success "Main binary extracted from build directory"
        else
            log_warning "Failed to extract main binary from build directory, trying staging area..."
            if docker cp "$TEMP_CONTAINER:/staging/opt/hokuyohub/hokuyo_hub" "$DIST_DIR/" 2>/dev/null; then
                log_success "Main binary extracted from staging area"
            else
                log_error "Failed to extract main binary"
                return 1
            fi
        fi
    fi
    
    # Extract configuration files
    log_info "Extracting configuration files..."
    if docker cp "$TEMP_CONTAINER:/opt/hokuyohub/configs" "$DIST_DIR/" 2>/dev/null; then
        log_success "Configuration files extracted successfully"
    else
        log_warning "Failed to extract config from runtime directory, trying staging..."
        if docker cp "$TEMP_CONTAINER:/staging/opt/hokuyohub/configs" "$DIST_DIR/" 2>/dev/null; then
            log_success "Configuration files extracted from staging area"
        else
            log_warning "Failed to extract config from staging, trying source..."
            if docker cp "$TEMP_CONTAINER:/build/configs" "$DIST_DIR/configs" 2>/dev/null; then
                log_success "Configuration files extracted from source"
            else
                log_warning "No configuration files found"
            fi
        fi
    fi
    
    # Extract WebUI files
    log_info "Extracting WebUI files..."
    if docker cp "$TEMP_CONTAINER:/opt/hokuyohub/webui" "$DIST_DIR/" 2>/dev/null; then
        log_success "WebUI files extracted successfully"
    else
        log_warning "Failed to extract webui from runtime directory, trying staging..."
        if docker cp "$TEMP_CONTAINER:/staging/opt/hokuyohub/webui" "$DIST_DIR/" 2>/dev/null; then
            log_success "WebUI files extracted from staging area"
        else
            log_warning "Failed to extract webui from staging, trying source..."
            if docker cp "$TEMP_CONTAINER:/build/webui" "$DIST_DIR/" 2>/dev/null; then
                log_success "WebUI files extracted from source"
            else
                log_warning "No WebUI files found"
            fi
        fi
    fi
    
    # Extract any additional libraries or dependencies
    log_info "Extracting additional dependencies..."
    docker cp "$TEMP_CONTAINER:/build/third_party" "$DIST_DIR/" 2>/dev/null || log_warning "No third_party directory found"
    
    # Create README for the dist folder
    log_info "Creating distribution README..."
    cat > "$DIST_DIR/README.md" << 'EOF'
# Hokuyo Hub - ARM64 Linux Distribution

This directory contains the compiled Hokuyo Hub application for ARM64 Linux (Raspberry Pi 5).

## Directory Structure

This follows the platform-specific directory structure pattern:
- `dist/darwin-arm64/` - macOS ARM64 builds
- `dist/linux-arm64/` - Linux ARM64 builds (this directory)

## Contents

- `hokuyo_hub` - Main application binary (ARM64 Linux)
- `configs/` - Configuration files
- `webui/` - Web user interface files
- `third_party/` - Third-party libraries and dependencies (if any)

## Installation

1. Copy all files to your target ARM64 Linux system (e.g., Raspberry Pi 5)
2. Make the binary executable: `chmod +x hokuyo_hub`
3. Run the application: `./hokuyo_hub`

## Requirements

- ARM64 Linux system (tested on Raspberry Pi 5)
- Required system libraries (see Docker runtime dependencies)

## Build Information

- Built using Docker multi-stage build
- Target platform: linux/arm64
- Build date: $(date -u +"%Y-%m-%d %H:%M:%S UTC")
- URG library: Rebuilt from source for ARM64 Linux
- Platform-specific output: dist/linux-arm64/

EOF
    
    log_success "Distribution README created"
}

# Validate extracted artifacts
validate_artifacts() {
    log_info "Validating extracted artifacts..."
    
    # Check main binary
    if [ -f "$DIST_DIR/hokuyo_hub" ]; then
        local size=$(stat -f%z "$DIST_DIR/hokuyo_hub" 2>/dev/null || stat -c%s "$DIST_DIR/hokuyo_hub" 2>/dev/null || echo "unknown")
        log_success "Main binary: hokuyo_hub ($size bytes)"
        
        # Check if it's ARM64
        if command -v file >/dev/null 2>&1; then
            local arch=$(file "$DIST_DIR/hokuyo_hub" | grep -o "ARM aarch64" || echo "unknown")
            if [ "$arch" = "ARM aarch64" ]; then
                log_success "Binary architecture: ARM64 ✓"
            else
                log_warning "Binary architecture: $arch"
            fi
        fi
    else
        log_error "Main binary not found!"
        return 1
    fi
    
    # Check config directory
    if [ -d "$DIST_DIR/configs" ]; then
        local config_count=$(find "$DIST_DIR/configs" -type f | wc -l)
        log_success "Configuration files: $config_count files"
    else
        log_warning "Configuration directory not found"
    fi
    
    # Check webui directory
    if [ -d "$DIST_DIR/webui" ]; then
        local webui_count=$(find "$DIST_DIR/webui" -type f | wc -l)
        log_success "WebUI files: $webui_count files"
    else
        log_warning "WebUI directory not found"
    fi
    
    # Calculate total size
    if command -v du >/dev/null 2>&1; then
        local total_size=$(du -sh "$DIST_DIR" | cut -f1)
        log_success "Total distribution size: $total_size"
    fi
    
    # Generate file manifest
    log_info "Generating file manifest..."
    find "$DIST_DIR" -type f -exec ls -la {} \; > "$DIST_DIR/MANIFEST.txt"
    log_success "File manifest created: MANIFEST.txt"
    
    # Generate checksums
    if command -v sha256sum >/dev/null 2>&1; then
        log_info "Generating checksums..."
        find "$DIST_DIR" -type f -name "hokuyo_hub" -exec sha256sum {} \; > "$DIST_DIR/CHECKSUMS.txt"
        log_success "Checksums created: CHECKSUMS.txt"
    elif command -v shasum >/dev/null 2>&1; then
        log_info "Generating checksums..."
        find "$DIST_DIR" -type f -name "hokuyo_hub" -exec shasum -a 256 {} \; > "$DIST_DIR/CHECKSUMS.txt"
        log_success "Checksums created: CHECKSUMS.txt"
    fi
}

# Show final results
show_results() {
    log_success "=== Docker Build Artifact Extraction Complete ==="
    log_info "Distribution location: $DIST_DIR"
    log_info "Contents:"
    ls -la "$DIST_DIR"
    
    if [ -f "$DIST_DIR/MANIFEST.txt" ]; then
        echo
        log_info "Complete file manifest:"
        cat "$DIST_DIR/MANIFEST.txt"
    fi
    
    if [ -f "$DIST_DIR/CHECKSUMS.txt" ]; then
        echo
        log_info "Binary checksums:"
        cat "$DIST_DIR/CHECKSUMS.txt"
    fi
}

# Main execution
main() {
    log_info "Docker Artifact Extraction Script for Hokuyo Hub"
    log_info "=============================================="
    
    extract_artifacts
    validate_artifacts
    show_results
    
    log_success "Artifact extraction completed successfully!"
    log_info "The dist/linux-arm64/ folder is ready for deployment to ARM64 Linux systems."
    log_info "This follows the established platform-specific directory structure pattern."
}

# Run main function
main "$@"