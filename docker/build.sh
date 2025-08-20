#!/bin/bash
# =============================================================================
# Docker Build Script for Hokuyo Hub - Raspberry Pi ARM64
# =============================================================================
# This script provides convenient commands for building and testing the Docker
# images for Raspberry Pi ARM64 target.
#
# Usage:
#   ./docker/build.sh [command] [options]
#
# Commands:
#   build-deps    - Build only the dependencies stage
#   build-app     - Build the application (includes dependencies)
#   build-runtime - Build the complete runtime image
#   build-all     - Build all stages
#   test-build    - Test the build process
#   test-run      - Test running the container
#   clean         - Clean up Docker images and containers
#   help          - Show this help message

set -e

# Configuration
DOCKER_REGISTRY="${DOCKER_REGISTRY:-}"
IMAGE_NAME="${IMAGE_NAME:-hokuyo-hub}"
IMAGE_TAG="${IMAGE_TAG:-latest}"
PLATFORM="${PLATFORM:-linux/arm64}"
DOCKERFILE="${DOCKERFILE:-docker/Dockerfile.rpi}"

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

# Check if Docker Buildx is available
check_buildx() {
    if ! docker buildx version >/dev/null 2>&1; then
        log_error "Docker Buildx is required but not available"
        log_info "Please install Docker Buildx or use Docker Desktop"
        exit 1
    fi
    
    # Create builder if it doesn't exist
    if ! docker buildx inspect hokuyo-builder >/dev/null 2>&1; then
        log_info "Creating Docker Buildx builder..."
        docker buildx create --name hokuyo-builder --use
    else
        docker buildx use hokuyo-builder
    fi
}

# Build dependencies stage
build_deps() {
    log_info "Building dependencies stage..."
    docker buildx build \
        --platform "${PLATFORM}" \
        --target build-deps \
        --tag "${IMAGE_NAME}:deps-${IMAGE_TAG}" \
        --file "${DOCKERFILE}" \
        --progress=plain \
        .
    log_success "Dependencies stage built successfully"
}

# Build application stage
build_app() {
    log_info "Building application stage..."
    docker buildx build \
        --platform "${PLATFORM}" \
        --target build-app \
        --tag "${IMAGE_NAME}:build-${IMAGE_TAG}" \
        --file "${DOCKERFILE}" \
        --progress=plain \
        .
    log_success "Application stage built successfully"
}

# Build runtime stage
build_runtime() {
    log_info "Building runtime stage..."
    docker buildx build \
        --platform "${PLATFORM}" \
        --target runtime \
        --tag "${IMAGE_NAME}:${IMAGE_TAG}" \
        --file "${DOCKERFILE}" \
        --progress=plain \
        --load \
        .
    log_success "Runtime stage built successfully"
}

# Build all stages
build_all() {
    log_info "Building all stages..."
    build_deps
    build_app
    build_runtime
    log_success "All stages built successfully"
}

# Test build process
test_build() {
    log_info "Testing build process..."
    
    # Build with verbose output
    docker buildx build \
        --platform "${PLATFORM}" \
        --target build-app \
        --tag "${IMAGE_NAME}:test-build" \
        --file "${DOCKERFILE}" \
        --progress=plain \
        .
    
    # Extract build artifacts for inspection
    log_info "Extracting build artifacts..."
    docker create --name temp-container "${IMAGE_NAME}:test-build"
    docker cp temp-container:/build/build/linux-arm64/hokuyo_hub ./hokuyo_hub.test || true
    docker rm temp-container
    
    if [ -f "./hokuyo_hub.test" ]; then
        log_info "Build artifact details:"
        file ./hokuyo_hub.test
        ls -la ./hokuyo_hub.test
        rm ./hokuyo_hub.test
        log_success "Build test completed successfully"
    else
        log_error "Build test failed - no artifact found"
        exit 1
    fi
}

# Test running the container
test_run() {
    log_info "Testing container execution..."
    
    # Build runtime image first
    build_runtime
    
    # Run container with health check
    log_info "Starting container for testing..."
    docker run --rm -d \
        --name hokuyo-test \
        --platform "${PLATFORM}" \
        -p 8080:8080 \
        "${IMAGE_NAME}:${IMAGE_TAG}"
    
    # Wait for container to start
    log_info "Waiting for container to start..."
    sleep 10
    
    # Check if container is running
    if docker ps | grep -q hokuyo-test; then
        log_success "Container started successfully"
        
        # Test health endpoint (if available)
        if command -v curl >/dev/null 2>&1; then
            log_info "Testing health endpoint..."
            if curl -f http://localhost:8080/api/health >/dev/null 2>&1; then
                log_success "Health check passed"
            else
                log_warning "Health check failed (endpoint may not be implemented)"
            fi
        fi
        
        # Show container logs
        log_info "Container logs:"
        docker logs hokuyo-test
        
        # Stop container
        docker stop hokuyo-test
        log_success "Container test completed"
    else
        log_error "Container failed to start"
        docker logs hokuyo-test || true
        docker stop hokuyo-test || true
        exit 1
    fi
}

# Clean up Docker images and containers
clean() {
    log_info "Cleaning up Docker images and containers..."
    
    # Stop and remove test containers
    docker stop hokuyo-test >/dev/null 2>&1 || true
    docker rm hokuyo-test >/dev/null 2>&1 || true
    
    # Remove images
    docker rmi "${IMAGE_NAME}:${IMAGE_TAG}" >/dev/null 2>&1 || true
    docker rmi "${IMAGE_NAME}:build-${IMAGE_TAG}" >/dev/null 2>&1 || true
    docker rmi "${IMAGE_NAME}:deps-${IMAGE_TAG}" >/dev/null 2>&1 || true
    docker rmi "${IMAGE_NAME}:test-build" >/dev/null 2>&1 || true
    
    # Clean up build cache
    docker buildx prune -f >/dev/null 2>&1 || true
    
    log_success "Cleanup completed"
}

# Show help
show_help() {
    echo "Docker Build Script for Hokuyo Hub - Raspberry Pi ARM64"
    echo ""
    echo "Usage: $0 [command] [options]"
    echo ""
    echo "Commands:"
    echo "  build-deps    - Build only the dependencies stage"
    echo "  build-app     - Build the application (includes dependencies)"
    echo "  build-runtime - Build the complete runtime image"
    echo "  build-all     - Build all stages"
    echo "  test-build    - Test the build process"
    echo "  test-run      - Test running the container"
    echo "  clean         - Clean up Docker images and containers"
    echo "  help          - Show this help message"
    echo ""
    echo "Environment Variables:"
    echo "  DOCKER_REGISTRY - Docker registry prefix (optional)"
    echo "  IMAGE_NAME      - Image name (default: hokuyo-hub)"
    echo "  IMAGE_TAG       - Image tag (default: latest)"
    echo "  PLATFORM        - Target platform (default: linux/arm64)"
    echo "  DOCKERFILE      - Dockerfile path (default: docker/Dockerfile.rpi)"
    echo ""
    echo "Examples:"
    echo "  $0 build-all"
    echo "  $0 test-build"
    echo "  IMAGE_TAG=v1.0.0 $0 build-runtime"
    echo "  PLATFORM=linux/amd64 $0 build-all"
}

# Main script logic
main() {
    case "${1:-help}" in
        build-deps)
            check_buildx
            build_deps
            ;;
        build-app)
            check_buildx
            build_app
            ;;
        build-runtime)
            check_buildx
            build_runtime
            ;;
        build-all)
            check_buildx
            build_all
            ;;
        test-build)
            check_buildx
            test_build
            ;;
        test-run)
            check_buildx
            test_run
            ;;
        clean)
            clean
            ;;
        help|--help|-h)
            show_help
            ;;
        *)
            log_error "Unknown command: $1"
            show_help
            exit 1
            ;;
    esac
}

# Run main function with all arguments
main "$@"