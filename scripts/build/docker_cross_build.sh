#!/bin/bash
# =============================================================================
# Docker Cross-Compilation Build Wrapper for Raspberry Pi 5
# =============================================================================
# This script provides a unified interface to the existing Docker build system
# located in the docker/ directory. It maintains compatibility with the script
# reorganization while leveraging the proven Docker build infrastructure.
#
# Usage:
#   ./scripts/build/docker_cross_build.sh [OPTIONS]
#
# Examples:
#   ./scripts/build/docker_cross_build.sh --build-all
#   ./scripts/build/docker_cross_build.sh --test-build
#   ./scripts/build/docker_cross_build.sh --extract

set -euo pipefail

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Configuration
DOCKER_BUILD_SCRIPT="./docker/build.sh"
EXTRACT_SCRIPT="./scripts/utils/extract_docker_artifacts.sh"

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

print_success() {
    print_status "$GREEN" "SUCCESS: $1"
}

# Function to show usage
show_usage() {
    cat << EOF
Docker Cross-Compilation Build Wrapper for Raspberry Pi 5

This script provides a unified interface to the existing Docker build system.
It leverages the proven multi-stage Docker build infrastructure in docker/.

Usage: $0 [OPTIONS]

Build Options:
    --build-deps       Build only the dependencies stage
    --build-app        Build the application (includes dependencies)
    --build-runtime    Build the complete runtime image
    --build-all        Build all stages (recommended)
    
Test Options:
    --test-build       Test the build process and extract artifacts
    --test-run         Test running the container with health checks
    
Utility Options:
    --extract          Extract build artifacts to dist/linux-arm64/
    --clean            Clean up Docker images and containers
    -h, --help         Show this help message

Environment Variables:
    IMAGE_TAG          Docker image tag (default: latest)
    PLATFORM           Target platform (default: linux/arm64)

Examples:
    $0 --build-all                    # Build complete Docker image
    $0 --test-build                   # Build and extract for testing
    $0 --extract                      # Extract existing artifacts
    IMAGE_TAG=v1.0.0 $0 --build-all  # Build with custom tag

Benefits of Docker approach:
    - Complete ARM64 Linux environment with Debian Bookworm
    - Multi-stage build for optimized final image
    - All dependencies managed through apt packages
    - Consistent, reproducible builds
    - No complex cross-compilation toolchain setup
    - CI/CD ready with automated testing

For detailed documentation, see: docker/README.md

EOF
}

# Function to check prerequisites
check_prerequisites() {
    # Check if Docker build script exists
    if [[ ! -f "$DOCKER_BUILD_SCRIPT" ]]; then
        print_error "Docker build script not found: $DOCKER_BUILD_SCRIPT"
        return 1
    fi
    
    # Make sure it's executable
    if [[ ! -x "$DOCKER_BUILD_SCRIPT" ]]; then
        print_status "$YELLOW" "Making Docker build script executable..."
        chmod +x "$DOCKER_BUILD_SCRIPT"
    fi
    
    print_success "Prerequisites check passed"
    return 0
}

# Function to extract artifacts after build
extract_artifacts() {
    print_header "Extracting Build Artifacts"
    
    local image_name="${IMAGE_NAME:-hokuyo-hub}"
    local image_tag="${IMAGE_TAG:-latest}"
    local full_image="${image_name}:${image_tag}"
    
    if [[ -x "$EXTRACT_SCRIPT" ]]; then
        print_status "$BLUE" "Using extraction script: $EXTRACT_SCRIPT"
        "$EXTRACT_SCRIPT" "$full_image"
    else
        print_status "$BLUE" "Manual artifact extraction for: $full_image"
        
        # Manual extraction as fallback
        mkdir -p dist/linux-arm64
        
        print_status "$BLUE" "Creating temporary container..."
        docker create --name temp-extract --platform linux/arm64 "$full_image" >/dev/null
        
        print_status "$BLUE" "Extracting artifacts..."
        docker cp temp-extract:/opt/hokuyohub/ dist/linux-arm64/ 2>/dev/null || \
        docker cp temp-extract:/opt/hokuyo-hub/ dist/linux-arm64/ 2>/dev/null || {
            print_error "Failed to extract artifacts from container"
            docker rm temp-extract >/dev/null 2>&1 || true
            return 1
        }
        
        print_status "$BLUE" "Cleaning up temporary container..."
        docker rm temp-extract >/dev/null
    fi
    
    # Verify extracted artifacts
    if [[ -d "dist/linux-arm64" ]] && [[ -n "$(ls -A dist/linux-arm64 2>/dev/null)" ]]; then
        print_success "Artifacts extracted to dist/linux-arm64/"
        
        # Show contents
        print_status "$BLUE" "Extracted files:"
        ls -la dist/linux-arm64/ | head -10
        
        # Verify binary
        local binary_path=$(find dist/linux-arm64 -name "hokuyo_hub" -type f 2>/dev/null | head -1)
        if [[ -n "$binary_path" ]]; then
            print_status "$BLUE" "Binary verification:"
            file "$binary_path"
        fi
        
        return 0
    else
        print_error "Artifact extraction failed - no files found"
        return 1
    fi
}

# Main script execution
main() {
    local action=""
    local docker_command=""
    local extract_after=false
    
    # Parse command line arguments
    while [[ $# -gt 0 ]]; do
        case $1 in
            --build-deps)
                action="build"
                docker_command="build-deps"
                shift
                ;;
            --build-app)
                action="build"
                docker_command="build-app"
                extract_after=true
                shift
                ;;
            --build-runtime)
                action="build"
                docker_command="build-runtime"
                extract_after=true
                shift
                ;;
            --build-all)
                action="build"
                docker_command="build-all"
                extract_after=true
                shift
                ;;
            --test-build)
                action="build"
                docker_command="test-build"
                extract_after=true
                shift
                ;;
            --test-run)
                action="build"
                docker_command="test-run"
                shift
                ;;
            --extract)
                action="extract"
                shift
                ;;
            --clean)
                action="build"
                docker_command="clean"
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
    
    # Default action if none specified
    if [[ -z "$action" ]]; then
        print_status "$YELLOW" "No action specified, defaulting to --build-all"
        action="build"
        docker_command="build-all"
        extract_after=true
    fi
    
    print_header "Docker Cross-Compilation Build for Raspberry Pi 5"
    
    # Check prerequisites
    if ! check_prerequisites; then
        exit 1
    fi
    
    case "$action" in
        build)
            print_status "$BLUE" "Delegating to Docker build system: $DOCKER_BUILD_SCRIPT $docker_command"
            if "$DOCKER_BUILD_SCRIPT" "$docker_command"; then
                print_success "Docker build completed: $docker_command"
                
                # Extract artifacts if requested
                if [[ "$extract_after" == true ]]; then
                    if extract_artifacts; then
                        print_success "Build and extraction completed successfully"
                    else
                        print_error "Build succeeded but extraction failed"
                        exit 1
                    fi
                fi
            else
                print_error "Docker build failed: $docker_command"
                exit 1
            fi
            ;;
        extract)
            if extract_artifacts; then
                print_success "Artifact extraction completed"
            else
                print_error "Artifact extraction failed"
                exit 1
            fi
            ;;
    esac
    
    print_header "Operation Complete"
    print_success "Docker build wrapper completed successfully!"
    
    if [[ -d "dist/linux-arm64" ]]; then
        print_status "$BLUE" ""
        print_status "$BLUE" "Next steps:"
        print_status "$BLUE" "1. Copy dist/linux-arm64/ to your Raspberry Pi"
        print_status "$BLUE" "2. Run: ./hokuyo_hub --config config/default.yaml"
        print_status "$BLUE" ""
        print_status "$BLUE" "For detailed usage: docker/README.md"
    fi
}

# Run main function with all arguments
main "$@"