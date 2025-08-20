# =============================================================================
# CMake Toolchain File for Raspberry Pi Cross-Compilation
# =============================================================================
# This is a convenience wrapper that includes the linux-arm64 toolchain
# specifically configured for Raspberry Pi 5.
#
# This file exists to maintain compatibility with the CMakePresets.json
# configuration that references "cmake/toolchains/rpi.cmake".

message(STATUS "Loading Raspberry Pi toolchain (wrapper for linux-arm64)")

# Include the main linux-arm64 toolchain
include(${CMAKE_CURRENT_LIST_DIR}/linux-arm64.cmake)

message(STATUS "Raspberry Pi toolchain loaded successfully")