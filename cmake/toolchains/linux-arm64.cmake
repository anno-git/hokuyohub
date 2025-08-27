# =============================================================================
# CMake Toolchain File for Linux ARM64 Cross-Compilation (Raspberry Pi 5)
# =============================================================================
# This toolchain enables cross-compilation from macOS (Apple Silicon) to 
# linux-arm64 targeting Raspberry Pi 5.
#
# Usage:
#   cmake -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/linux-arm64.cmake ..
#
# Requirements:
#   - aarch64-linux-gnu cross-compilation toolchain
#   - Optional: target sysroot for better library resolution
#
# Environment Variables (optional):
#   - CROSS_COMPILE_PREFIX: Override default aarch64-linux-gnu- prefix
#   - TARGET_SYSROOT: Path to target system root directory
#   - TOOLCHAIN_ROOT: Path to toolchain installation directory

# Include common utilities
include(${CMAKE_CURRENT_LIST_DIR}/common.cmake)

# =============================================================================
# Target System Configuration
# =============================================================================

set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_SYSTEM_VERSION 1)

# Set the target architecture for pkg-config and other tools
set(TARGET_ARCH "aarch64-linux-gnu")

message(STATUS "Configuring for Linux ARM64 (Raspberry Pi 5) cross-compilation")

# =============================================================================
# Toolchain Configuration
# =============================================================================

# Allow environment override of cross-compile prefix
if(DEFINED ENV{CROSS_COMPILE_PREFIX})
    set(CROSS_COMPILE_PREFIX $ENV{CROSS_COMPILE_PREFIX})
else()
    set(CROSS_COMPILE_PREFIX "aarch64-linux-gnu-")
endif()

# Allow environment override of toolchain root
if(DEFINED ENV{TOOLCHAIN_ROOT})
    set(TOOLCHAIN_ROOT $ENV{TOOLCHAIN_ROOT})
    list(APPEND CMAKE_PROGRAM_PATH "${TOOLCHAIN_ROOT}/bin")
endif()

message(STATUS "Using cross-compile prefix: ${CROSS_COMPILE_PREFIX}")

# =============================================================================
# Compiler Detection and Validation
# =============================================================================

# Define possible prefixes for tool search
set(TOOL_PREFIXES
    "${CROSS_COMPILE_PREFIX}"
    "aarch64-linux-gnu-"
    "aarch64-linux-musl-"
    "aarch64-unknown-linux-gnu-"
)

# Find cross-compilation tools
find_cross_tool(CMAKE_C_COMPILER "gcc" "${TOOL_PREFIXES}")
find_cross_tool(CMAKE_CXX_COMPILER "g++" "${TOOL_PREFIXES}")
find_cross_tool(CMAKE_AR "ar" "${TOOL_PREFIXES}")
find_cross_tool(CMAKE_RANLIB "ranlib" "${TOOL_PREFIXES}")
find_cross_tool(CMAKE_STRIP "strip" "${TOOL_PREFIXES}")
find_cross_tool(CMAKE_NM "nm" "${TOOL_PREFIXES}")

# Set make program for build system
find_program(CMAKE_MAKE_PROGRAM NAMES make gmake)
if(CMAKE_MAKE_PROGRAM)
    message(STATUS "Found make program: ${CMAKE_MAKE_PROGRAM}")
else()
    message(FATAL_ERROR "Could not find make program")
endif()

# Find optional tools (don't fail if not found)
find_program(CMAKE_OBJCOPY NAMES
    ${CROSS_COMPILE_PREFIX}objcopy
    aarch64-linux-gnu-objcopy
    aarch64-linux-musl-objcopy
    aarch64-unknown-linux-gnu-objcopy
    objcopy
)
find_program(CMAKE_OBJDUMP NAMES
    ${CROSS_COMPILE_PREFIX}objdump
    aarch64-linux-gnu-objdump
    aarch64-linux-musl-objdump
    aarch64-unknown-linux-gnu-objdump
    objdump
)

# Find additional tools for ExternalProject (optional)
find_program(CROSS_AS NAMES
    ${CROSS_COMPILE_PREFIX}as
    aarch64-linux-gnu-as
    aarch64-linux-musl-as
    aarch64-unknown-linux-gnu-as
    as
)
find_program(CROSS_LD NAMES
    ${CROSS_COMPILE_PREFIX}ld
    aarch64-linux-gnu-ld
    aarch64-linux-musl-ld
    aarch64-unknown-linux-gnu-ld
    ld
)

# Validate essential tools
validate_tool(CMAKE_C_COMPILER "C compiler")
validate_tool(CMAKE_CXX_COMPILER "C++ compiler")
validate_tool(CMAKE_AR "AR archiver")
validate_tool(CMAKE_RANLIB "RANLIB")

# =============================================================================
# Sysroot Configuration
# =============================================================================

# Check for sysroot in environment or common locations
if(DEFINED ENV{TARGET_SYSROOT})
    set(TARGET_SYSROOT $ENV{TARGET_SYSROOT})
elseif(TOOLCHAIN_ROOT AND EXISTS "${TOOLCHAIN_ROOT}/${TARGET_ARCH}/sysroot")
    set(TARGET_SYSROOT "${TOOLCHAIN_ROOT}/${TARGET_ARCH}/sysroot")
elseif(EXISTS "/opt/homebrew/Cellar/aarch64-linux-gnu-gcc")
    # Try to find Homebrew installed sysroot (GNU toolchain)
    file(GLOB HOMEBREW_GCC_VERSIONS "/opt/homebrew/Cellar/aarch64-linux-gnu-gcc/*")
    if(HOMEBREW_GCC_VERSIONS)
        list(GET HOMEBREW_GCC_VERSIONS -1 LATEST_VERSION)
        set(TARGET_SYSROOT "${LATEST_VERSION}/toolchain/${TARGET_ARCH}/sysroot")
    endif()
elseif(EXISTS "/opt/homebrew/Cellar/musl-cross")
    # Try to find Homebrew installed musl-cross sysroot
    file(GLOB MUSL_CROSS_VERSIONS "/opt/homebrew/Cellar/musl-cross/*")
    if(MUSL_CROSS_VERSIONS)
        list(GET MUSL_CROSS_VERSIONS -1 LATEST_VERSION)
        set(TARGET_SYSROOT "${LATEST_VERSION}/libexec/aarch64-linux-musl")
    endif()
endif()

# Configure sysroot if available
if(TARGET_SYSROOT AND EXISTS "${TARGET_SYSROOT}")
    validate_sysroot("${TARGET_SYSROOT}")
    set(CMAKE_SYSROOT "${TARGET_SYSROOT}")
    set(CMAKE_FIND_ROOT_PATH "${TARGET_SYSROOT}")
    
    # Set up pkg-config for cross-compilation
    setup_cross_pkg_config("${TARGET_ARCH}" "${TARGET_SYSROOT}")
    
    message(STATUS "Using sysroot: ${CMAKE_SYSROOT}")
else()
    message(WARNING "No sysroot found. Library detection may be limited.")
    message(STATUS "To improve library detection, set TARGET_SYSROOT environment variable")
    message(STATUS "Example: export TARGET_SYSROOT=/path/to/rpi/sysroot")
endif()

# =============================================================================
# Compiler and Linker Flags
# =============================================================================

# CPU-specific optimizations for Raspberry Pi 5 (Cortex-A76)
set(RPI5_CPU_FLAGS "-mcpu=cortex-a76 -mtune=cortex-a76")

# Common flags for both C and C++
set(COMMON_FLAGS "${RPI5_CPU_FLAGS} -fPIC -fstack-protector-strong")

# C compiler flags
set(CMAKE_C_FLAGS_INIT "${COMMON_FLAGS}")
set(CMAKE_C_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_C_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_C_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")
set(CMAKE_C_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")

# C++ compiler flags
set(CMAKE_CXX_FLAGS_INIT "${COMMON_FLAGS}")
set(CMAKE_CXX_FLAGS_DEBUG_INIT "-g -O0")
set(CMAKE_CXX_FLAGS_RELEASE_INIT "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g -DNDEBUG")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT "-Os -DNDEBUG")

# Linker flags - conditional based on platform and toolchain
# Check if we're using a GNU-compatible linker that supports --as-needed
if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    # For cross-compilation to Linux, use GNU linker flags
    set(CMAKE_EXE_LINKER_FLAGS_INIT "-Wl,--as-needed")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "-Wl,--as-needed")
else()
    # For other platforms (like macOS host), don't use GNU-specific flags
    set(CMAKE_EXE_LINKER_FLAGS_INIT "")
    set(CMAKE_SHARED_LINKER_FLAGS_INIT "")
endif()

# =============================================================================
# ExternalProject Support (for urg_library)
# =============================================================================

# Set up environment variables for ExternalProject builds
setup_external_project_env(
    "${CMAKE_C_COMPILER}"
    "${CMAKE_CXX_COMPILER}"
    "${CMAKE_AR}"
    "${CMAKE_RANLIB}"
)

# Additional environment variables for Makefile-based builds
set(ENV{AS} "${CROSS_AS}")
set(ENV{LD} "${CROSS_LD}")
set(ENV{NM} "${CMAKE_NM}")
set(ENV{OBJCOPY} "${CMAKE_OBJCOPY}")
set(ENV{OBJDUMP} "${CMAKE_OBJDUMP}")
set(ENV{STRIP} "${CMAKE_STRIP}")

# Set CFLAGS and CXXFLAGS for external projects
set(ENV{CFLAGS} "${CMAKE_C_FLAGS_INIT} ${CMAKE_C_FLAGS_RELEASE_INIT}")
set(ENV{CXXFLAGS} "${CMAKE_CXX_FLAGS_INIT} ${CMAKE_CXX_FLAGS_RELEASE_INIT}")

# =============================================================================
# Find Root Path Configuration
# =============================================================================

# Configure search behavior for cross-compilation
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Add additional search paths if sysroot is available
if(CMAKE_SYSROOT)
    list(APPEND CMAKE_FIND_ROOT_PATH 
        "${CMAKE_SYSROOT}/usr"
        "${CMAKE_SYSROOT}/usr/local"
    )
endif()

# =============================================================================
# Package Configuration
# =============================================================================

# Help CMake find packages in the target environment
if(CMAKE_SYSROOT)
    # Add target-specific package paths
    list(APPEND CMAKE_PREFIX_PATH
        "${CMAKE_SYSROOT}/usr/lib/${TARGET_ARCH}/cmake"
        "${CMAKE_SYSROOT}/usr/lib/cmake"
        "${CMAKE_SYSROOT}/usr/share/cmake"
    )
endif()

# =============================================================================
# Cross-Compilation Dependency Support
# =============================================================================

# Include jsoncpp configuration for cross-compilation
include(${CMAKE_CURRENT_LIST_DIR}/jsoncpp-config.cmake)

# =============================================================================
# Validation and Summary
# =============================================================================

# Print configuration summary
print_toolchain_summary()

# Note: Compiler validation will happen during the normal CMake configure process
# when the project() command enables the C/C++ languages

message(STATUS "Linux ARM64 toolchain configuration complete")

# =============================================================================
# Usage Instructions
# =============================================================================

if(CMAKE_CROSSCOMPILING)
    message(STATUS "")
    message(STATUS "=== Cross-Compilation Setup Instructions ===")
    message(STATUS "")
    message(STATUS "1. Install cross-compilation toolchain:")
    message(STATUS "   macOS (Homebrew): brew install aarch64-linux-gnu-gcc")
    message(STATUS "   Ubuntu/Debian: apt-get install gcc-aarch64-linux-gnu")
    message(STATUS "")
    message(STATUS "2. Optional - Set up target sysroot:")
    message(STATUS "   export TARGET_SYSROOT=/path/to/raspberry-pi-sysroot")
    message(STATUS "")
    message(STATUS "3. Build with CMake preset:")
    message(STATUS "   cmake --preset rpi-base")
    message(STATUS "   cmake --build build/linux-arm64")
    message(STATUS "")
    message(STATUS "4. For urg_library ExternalProject, ensure these are set:")
    message(STATUS "   CC=${CMAKE_C_COMPILER}")
    message(STATUS "   CXX=${CMAKE_CXX_COMPILER}")
    message(STATUS "   AR=${CMAKE_AR}")
    message(STATUS "   RANLIB=${CMAKE_RANLIB}")
    message(STATUS "")
    message(STATUS "============================================")
endif()