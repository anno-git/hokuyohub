#!/bin/bash

# =============================================================================
# HokuyoHub Regression Detection Script
# =============================================================================
# This script provides comprehensive regression detection for the HokuyoHub
# cross-compilation build system implementation.
#
# Usage: ./regression_check.sh [options]
#
# Options:
#   --quick              Quick regression check (build + basic tests)
#   --full               Full regression check (build + API tests + artifacts)
#   --build-only         Only check build success
#   --api-only           Only run API tests (requires running server)
#   --server-url URL     Server URL for API tests (default: http://localhost:8080)
#   --timeout SECONDS    Timeout for server startup (default: 30)
#   --no-cleanup         Don't clean up temporary files
#   --verbose            Enable verbose output
#
# Exit codes:
#   0: No regressions detected
#   1: Build regression detected
#   2: API regression detected
#   3: Artifact regression detected
#   4: Server startup failed

set -euo pipefail

# Configuration
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
BASELINE_SCRIPT="$SCRIPT_DIR/baseline_verification.sh"
API_TEST_SCRIPT="$SCRIPT_DIR/test_rest_api.sh"
TEMP_DIR="$PROJECT_ROOT/.regression_temp"

# Default options
CHECK_MODE="quick"
SERVER_URL="http://localhost:8080"
SERVER_TIMEOUT=30
CLEANUP=true
VERBOSE=false

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
while [[ $# -gt 0 ]]; do
    case $1 in
        --quick)
            CHECK_MODE="quick"
            shift
            ;;
        --full)
            CHECK_MODE="full"
            shift
            ;;
        --build-only)
            CHECK_MODE="build-only"
            shift
            ;;
        --api-only)
            CHECK_MODE="api-only"
            shift
            ;;
        --server-url)
            SERVER_URL="$2"
            shift 2
            ;;
        --timeout)
            SERVER_TIMEOUT="$2"
            shift 2
            ;;
        --no-cleanup)
            CLEANUP=false
            shift
            ;;
        --verbose)
            VERBOSE=true
            shift
            ;;
        -h|--help)
            echo "Usage: $0 [--quick|--full|--build-only|--api-only] [options]"
            echo "Options:"
            echo "  --server-url URL     Server URL for API tests"
            echo "  --timeout SECONDS    Server startup timeout"
            echo "  --no-cleanup         Don't clean temporary files"
            echo "  --verbose            Enable verbose output"
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

# Cleanup function
cleanup() {
    if [[ "$CLEANUP" == "true" && -d "$TEMP_DIR" ]]; then
        rm -rf "$TEMP_DIR"
        verbose_log "Cleaned up temporary directory: $TEMP_DIR"
    fi
}

# Set up cleanup trap
trap cleanup EXIT

# Check if baseline exists
check_baseline() {
    if [[ ! -d "$PROJECT_ROOT/build/darwin-arm64/baseline" ]]; then
        log_error "No baseline found. Run baseline_verification.sh --capture-baseline first."
        exit 1
    fi
}

# Build regression check
check_build_regression() {
    log_info "Checking for build regressions..."
    
    local build_args=""
    if [[ "$VERBOSE" == "true" ]]; then
        build_args="--verbose"
    fi
    
    # Run baseline verification with comparison
    if "$BASELINE_SCRIPT" --compare-baseline $build_args; then
        log_success "No build regressions detected"
        return 0
    else
        local exit_code=$?
        if [[ $exit_code -eq 1 ]]; then
            log_error "Build failed - regression detected"
            return 1
        elif [[ $exit_code -eq 2 ]]; then
            log_warning "Build artifacts changed - potential regression"
            return 3
        else
            log_error "Baseline verification failed with exit code $exit_code"
            return 1
        fi
    fi
}

# Start server for API tests
start_server() {
    log_info "Starting HokuyoHub server for API tests..."
    
    local server_binary="$PROJECT_ROOT/dist/darwin-arm64/Release/hokuyo_hub"
    if [[ ! -f "$server_binary" ]]; then
        log_error "Server binary not found: $server_binary"
        return 4
    fi
    
    # Create temp directory for server logs
    mkdir -p "$TEMP_DIR"
    
    # Start server in background
    cd "$PROJECT_ROOT/dist/darwin-arm64/Release"
    nohup ./hokuyo_hub > "$TEMP_DIR/server.log" 2>&1 &
    local server_pid=$!
    echo $server_pid > "$TEMP_DIR/server.pid"
    
    verbose_log "Server started with PID: $server_pid"
    
    # Wait for server to be ready
    log_info "Waiting for server to start (timeout: ${SERVER_TIMEOUT}s)..."
    local count=0
    while [[ $count -lt $SERVER_TIMEOUT ]]; do
        if curl -s "$SERVER_URL/api/v1/snapshot" >/dev/null 2>&1; then
            log_success "Server is ready"
            return 0
        fi
        sleep 1
        ((count++))
    done
    
    log_error "Server failed to start within ${SERVER_TIMEOUT} seconds"
    
    # Show server logs for debugging
    if [[ -f "$TEMP_DIR/server.log" ]]; then
        log_info "Server logs:"
        tail -20 "$TEMP_DIR/server.log"
    fi
    
    return 4
}

# Stop server
stop_server() {
    if [[ -f "$TEMP_DIR/server.pid" ]]; then
        local server_pid=$(cat "$TEMP_DIR/server.pid")
        if kill -0 "$server_pid" 2>/dev/null; then
            verbose_log "Stopping server (PID: $server_pid)"
            kill "$server_pid" 2>/dev/null || true
            
            # Wait for graceful shutdown
            local count=0
            while [[ $count -lt 10 ]] && kill -0 "$server_pid" 2>/dev/null; do
                sleep 1
                ((count++))
            done
            
            # Force kill if still running
            if kill -0 "$server_pid" 2>/dev/null; then
                kill -9 "$server_pid" 2>/dev/null || true
            fi
        fi
        rm -f "$TEMP_DIR/server.pid"
    fi
}

# API regression check
check_api_regression() {
    log_info "Checking for API regressions..."
    
    # Check if server is already running
    local server_was_running=false
    if curl -s "$SERVER_URL/api/v1/snapshot" >/dev/null 2>&1; then
        log_info "Server already running at $SERVER_URL"
        server_was_running=true
    else
        # Start server
        if ! start_server; then
            return 4
        fi
    fi
    
    # Run API tests
    local api_result=0
    if [[ "$VERBOSE" == "true" ]]; then
        "$API_TEST_SCRIPT" "$SERVER_URL" || api_result=$?
    else
        "$API_TEST_SCRIPT" "$SERVER_URL" >/dev/null 2>&1 || api_result=$?
    fi
    
    # Stop server if we started it
    if [[ "$server_was_running" == "false" ]]; then
        stop_server
    fi
    
    if [[ $api_result -eq 0 ]]; then
        log_success "No API regressions detected"
        return 0
    else
        log_error "API tests failed - regression detected"
        return 2
    fi
}

# Quick regression check
quick_check() {
    log_info "Running quick regression check..."
    
    local build_result=0
    check_build_regression || build_result=$?
    
    if [[ $build_result -ne 0 ]]; then
        return $build_result
    fi
    
    # Basic smoke test - just check if binary runs
    log_info "Running basic smoke test..."
    if timeout 5 "$PROJECT_ROOT/dist/darwin-arm64/Release/hokuyo_hub" --help >/dev/null 2>&1; then
        log_success "Basic smoke test passed"
    else
        log_warning "Basic smoke test failed (binary may not support --help)"
    fi
    
    return 0
}

# Full regression check
full_check() {
    log_info "Running full regression check..."
    
    local build_result=0
    local api_result=0
    
    # Build check
    check_build_regression || build_result=$?
    
    if [[ $build_result -eq 1 ]]; then
        log_error "Build failed, skipping API tests"
        return $build_result
    fi
    
    # API check
    check_api_regression || api_result=$?
    
    # Return the most severe error
    if [[ $build_result -ne 0 ]]; then
        return $build_result
    elif [[ $api_result -ne 0 ]]; then
        return $api_result
    else
        return 0
    fi
}

# Main execution
main() {
    echo "=== HokuyoHub Regression Detection ==="
    echo "Mode: $CHECK_MODE"
    echo "Project: $PROJECT_ROOT"
    echo
    
    # Check baseline exists (except for api-only mode)
    if [[ "$CHECK_MODE" != "api-only" ]]; then
        check_baseline
    fi
    
    local exit_code=0
    
    case "$CHECK_MODE" in
        "quick")
            quick_check || exit_code=$?
            ;;
        "full")
            full_check || exit_code=$?
            ;;
        "build-only")
            check_build_regression || exit_code=$?
            ;;
        "api-only")
            check_api_regression || exit_code=$?
            ;;
        *)
            log_error "Unknown check mode: $CHECK_MODE"
            exit 1
            ;;
    esac
    
    echo
    case $exit_code in
        0)
            log_success "No regressions detected"
            ;;
        1)
            log_error "Build regression detected"
            ;;
        2)
            log_error "API regression detected"
            ;;
        3)
            log_warning "Artifact changes detected (potential regression)"
            ;;
        4)
            log_error "Server startup failed"
            ;;
        *)
            log_error "Unknown error occurred (exit code: $exit_code)"
            ;;
    esac
    
    return $exit_code
}

# Run main function
main "$@"