#!/bin/bash

# HokuyoHub Simplified Restart Script
# This script reads restart arguments from a temporary file and starts a new HokuyoHub process
# The application handles its own graceful shutdown

set -euo pipefail

# Configuration
SCRIPT_NAME="$(basename "$0")"
LOG_DIR="./logs"
LOG_FILE="${LOG_DIR}/restart.log"
EXECUTABLE="./dist/darwin-arm64/hokuyo_hub"
DEFAULT_CONFIG="./dist/darwin-arm64/configs/default.yaml"

# Temporary file candidates (in order of preference)
TMP_DIRS=("${TMPDIR:-}" "/tmp" "./tmp")
ARG_FILE_NAME="hokuyo_restart_args.txt"

# Function to log messages with timestamp
log_message() {
    local level="$1"
    shift
    local message="$*"
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] [$level] $message" | tee -a "$LOG_FILE"
}

# Function to log to file only (for detailed logs)
log_debug() {
    local timestamp=$(date '+%Y-%m-%d %H:%M:%S')
    echo "[$timestamp] [DEBUG] $*" >> "$LOG_FILE"
}

# Function to find and validate temporary file
find_args_file() {
    local args_file=""
    
    for tmp_dir in "${TMP_DIRS[@]}"; do
        if [[ -n "$tmp_dir" && -d "$tmp_dir" ]]; then
            local candidate="${tmp_dir}/${ARG_FILE_NAME}"
            if [[ -f "$candidate" && -r "$candidate" ]]; then
                args_file="$candidate"
                log_debug "Found arguments file: $args_file"
                break
            fi
        fi
    done
    
    echo "$args_file"
}

# Function to read and validate arguments from file
read_args_from_file() {
    local args_file="$1"
    local -a args=()
    
    if [[ ! -f "$args_file" ]]; then
        log_message "WARN" "Arguments file not found: $args_file"
        return 1
    fi
    
    log_debug "Reading arguments from: $args_file"
    
    # Read file line by line and handle escaped arguments
    while IFS= read -r line || [[ -n "$line" ]]; do
        # Skip empty lines and comments
        [[ -z "$line" || "$line" =~ ^[[:space:]]*# ]] && continue
        
        # Basic argument validation (no dangerous characters)
        if [[ "$line" =~ [\;\|\&\`\$\(\)] ]]; then
            log_message "ERROR" "Potentially dangerous argument detected: $line"
            return 1
        fi
        
        # Handle quoted arguments and escape sequences
        if [[ "$line" =~ ^\".*\"$ ]]; then
            # Remove surrounding quotes and handle escaped quotes
            line="${line#\"}"
            line="${line%\"}"
            line="${line//\\\"/\"}"
        fi
        
        args+=("$line")
        log_debug "Added argument: $line"
    done < "$args_file"
    
    if [[ ${#args[@]} -eq 0 ]]; then
        log_message "WARN" "No valid arguments found in file"
        return 1
    fi
    
    log_debug "Successfully read ${#args[@]} arguments from file"
    
    # Return arguments via stdout (caller should capture)
    printf '%s\n' "${args[@]}"
    return 0
}

# Function to wait for port availability (minimal approach)
wait_for_port_availability() {
    log_message "INFO" "Checking port 8080 availability..."
    
    # Simple port check - if available, proceed immediately
    if ! netstat -an | grep -q ":8080.*LISTEN"; then
        log_message "INFO" "Port 8080 is available"
        return 0
    fi
    
    # Brief wait for port to be released
    log_message "INFO" "Port 8080 in use, waiting for release..."
    local port_timeout=10
    while [[ $port_timeout -gt 0 ]]; do
        if ! netstat -an | grep -q ":8080.*LISTEN"; then
            log_message "INFO" "Port 8080 is now available"
            return 0
        fi
        sleep 1
        ((port_timeout--))
    done
    
    log_message "WARN" "Port 8080 still in use after 10 seconds, proceeding anyway"
    return 0
}

# Function to start the process with given arguments
start_process() {
    local -a args=("$@")
    
    if [[ ! -x "$EXECUTABLE" ]]; then
        log_message "ERROR" "Executable not found or not executable: $EXECUTABLE"
        return 1
    fi
    
    log_message "INFO" "Starting HokuyoHub with ${#args[@]} arguments"
    log_debug "Arguments: ${args[*]}"
    
    # Start the process directly (no PID file management)
    exec "$EXECUTABLE" "${args[@]}"
}

# Function to cleanup temporary files
cleanup_temp_files() {
    local args_file="$1"
    
    if [[ -n "$args_file" && -f "$args_file" ]]; then
        log_message "INFO" "Cleaning up temporary arguments file: $args_file"
        rm -f "$args_file" || log_message "WARN" "Failed to remove temporary file: $args_file"
    fi
}

# Main execution
main() {
    # Ensure log directory exists
    mkdir -p "$LOG_DIR"
    
    # Initialize log
    log_message "INFO" "=== HokuyoHub Restart Script Started ==="
    log_message "INFO" "Script: $SCRIPT_NAME"
    log_message "INFO" "Working directory: $(pwd)"
    
    # Find arguments file
    local args_file
    args_file=$(find_args_file)
    
    local -a restart_args=()
    
    if [[ -n "$args_file" ]]; then
        log_message "INFO" "Found arguments file: $args_file"
        
        # Read arguments from file (macOS compatible version)
        local args_output
        if args_output=$(read_args_from_file "$args_file"); then
            # Convert output to array using mapfile (more portable than readarray)
            local -a temp_args=()
            while IFS= read -r line; do
                temp_args+=("$line")
            done <<< "$args_output"
            restart_args=("${temp_args[@]}")
            log_message "INFO" "Successfully loaded restart arguments"
        else
            log_message "ERROR" "Failed to read arguments from file, using defaults"
            restart_args=()
        fi
        
        # Clean up the temporary file
        cleanup_temp_files "$args_file"
    else
        log_message "INFO" "No arguments file found, using default configuration"
    fi
    
    # Use default arguments if none provided
    if [[ ${#restart_args[@]} -eq 0 ]]; then
        if [[ -f "$DEFAULT_CONFIG" ]]; then
            restart_args=("--config" "$DEFAULT_CONFIG")
            log_message "INFO" "Using default configuration: $DEFAULT_CONFIG"
        else
            log_message "WARN" "Default configuration not found, starting without arguments"
            restart_args=()
        fi
    fi
    
    # Wait for port availability (minimal check)
    wait_for_port_availability
    
    # Start new process
    log_message "INFO" "Starting new HokuyoHub process"
    if start_process "${restart_args[@]}"; then
        log_message "INFO" "=== HokuyoHub Restart Completed Successfully ==="
        exit 0
    else
        log_message "ERROR" "=== HokuyoHub Restart Failed ==="
        exit 1
    fi
}

# Signal handlers for cleanup
trap 'log_message "ERROR" "Script interrupted"; exit 130' INT TERM

# Run main function
main "$@"