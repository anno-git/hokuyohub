#!/bin/bash

# =============================================================================
# Simple CMake Preset Wrapper - Phase 1
# =============================================================================
# Quick wrapper for common CMake preset operations
#
# Usage: ./preset_build.sh [debug|release|clean]

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

case "${1:-release}" in
    debug)
        echo "Building Debug with presets..."
        "$SCRIPT_DIR/../build/build_with_presets.sh" debug --install
        ;;
    release)
        echo "Building Release with presets..."
        "$SCRIPT_DIR/../build/build_with_presets.sh" release --install
        ;;
    clean)
        echo "Cleaning build directories..."
        "$SCRIPT_DIR/../build/build_with_presets.sh" clean
        ;;
    *)
        echo "Usage: $0 [debug|release|clean]"
        echo "  debug   - Build Debug configuration"
        echo "  release - Build Release configuration (default)"
        echo "  clean   - Clean build directories"
        exit 1
        ;;
esac