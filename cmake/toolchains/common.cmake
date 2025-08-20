# =============================================================================
# Common CMake Toolchain Utilities
# =============================================================================
# This file provides shared utilities and functions for all toolchain files.
# It should be included by specific toolchain files to access common functionality.

# Prevent multiple inclusions
if(TOOLCHAIN_COMMON_INCLUDED)
    return()
endif()
set(TOOLCHAIN_COMMON_INCLUDED TRUE)

# =============================================================================
# Utility Functions
# =============================================================================

# Function to validate that a required tool exists
function(validate_tool TOOL_VAR TOOL_NAME)
    if(NOT ${TOOL_VAR})
        message(FATAL_ERROR "Cross-compilation tool not found: ${TOOL_NAME}")
    endif()
    
    if(NOT EXISTS "${${TOOL_VAR}}")
        message(FATAL_ERROR "Cross-compilation tool does not exist: ${${TOOL_VAR}}")
    endif()
    
    message(STATUS "Found ${TOOL_NAME}: ${${TOOL_VAR}}")
endfunction()

# Function to find cross-compilation tools with multiple search strategies
function(find_cross_tool OUTPUT_VAR TOOL_NAME PREFIXES)
    # First try to find the tool with prefixes
    foreach(PREFIX ${PREFIXES})
        find_program(${OUTPUT_VAR} NAMES ${PREFIX}${TOOL_NAME})
        if(${OUTPUT_VAR})
            break()
        endif()
    endforeach()
    
    # If not found with prefixes, try without prefix
    if(NOT ${OUTPUT_VAR})
        find_program(${OUTPUT_VAR} NAMES ${TOOL_NAME})
    endif()
    
    # Validate the tool was found
    if(${OUTPUT_VAR})
        message(STATUS "Found cross-compilation ${TOOL_NAME}: ${${OUTPUT_VAR}}")
    else()
        message(FATAL_ERROR "Could not find cross-compilation ${TOOL_NAME}. Tried prefixes: ${PREFIXES}")
    endif()
endfunction()

# Function to set up pkg-config for cross-compilation
function(setup_cross_pkg_config TARGET_ARCH SYSROOT_PATH)
    # Set PKG_CONFIG_PATH to empty to avoid host contamination
    set(ENV{PKG_CONFIG_PATH} "")
    
    # Set PKG_CONFIG_LIBDIR to target system paths
    if(SYSROOT_PATH)
        set(ENV{PKG_CONFIG_LIBDIR} "${SYSROOT_PATH}/usr/lib/pkgconfig:${SYSROOT_PATH}/usr/share/pkgconfig:${SYSROOT_PATH}/usr/lib/${TARGET_ARCH}/pkgconfig")
        set(ENV{PKG_CONFIG_SYSROOT_DIR} "${SYSROOT_PATH}")
    endif()
    
    message(STATUS "PKG_CONFIG_LIBDIR: $ENV{PKG_CONFIG_LIBDIR}")
    message(STATUS "PKG_CONFIG_SYSROOT_DIR: $ENV{PKG_CONFIG_SYSROOT_DIR}")
endfunction()

# Function to set up ExternalProject environment variables for cross-compilation
function(setup_external_project_env CC_COMPILER CXX_COMPILER AR_TOOL RANLIB_TOOL)
    # Set environment variables that ExternalProject can use
    set(ENV{CC} "${CC_COMPILER}")
    set(ENV{CXX} "${CXX_COMPILER}")
    set(ENV{AR} "${AR_TOOL}")
    set(ENV{RANLIB} "${RANLIB_TOOL}")
    
    # Also set CMake cache variables for ExternalProject
    set(EXTERNAL_PROJECT_CC "${CC_COMPILER}" CACHE STRING "C compiler for ExternalProject")
    set(EXTERNAL_PROJECT_CXX "${CXX_COMPILER}" CACHE STRING "C++ compiler for ExternalProject")
    set(EXTERNAL_PROJECT_AR "${AR_TOOL}" CACHE STRING "AR tool for ExternalProject")
    set(EXTERNAL_PROJECT_RANLIB "${RANLIB_TOOL}" CACHE STRING "RANLIB tool for ExternalProject")
    
    message(STATUS "ExternalProject environment configured:")
    message(STATUS "  CC: ${CC_COMPILER}")
    message(STATUS "  CXX: ${CXX_COMPILER}")
    message(STATUS "  AR: ${AR_TOOL}")
    message(STATUS "  RANLIB: ${RANLIB_TOOL}")
endfunction()

# Function to validate sysroot directory
function(validate_sysroot SYSROOT_PATH)
    if(NOT EXISTS "${SYSROOT_PATH}")
        message(FATAL_ERROR "Sysroot directory does not exist: ${SYSROOT_PATH}")
    endif()
    
    # Check for essential directories
    set(ESSENTIAL_DIRS "usr/include" "usr/lib" "lib")
    foreach(DIR ${ESSENTIAL_DIRS})
        if(NOT EXISTS "${SYSROOT_PATH}/${DIR}")
            message(WARNING "Sysroot missing directory: ${SYSROOT_PATH}/${DIR}")
        endif()
    endforeach()
    
    message(STATUS "Sysroot validated: ${SYSROOT_PATH}")
endfunction()

# Function to print toolchain summary
function(print_toolchain_summary)
    message(STATUS "=== Cross-Compilation Toolchain Summary ===")
    message(STATUS "System Name: ${CMAKE_SYSTEM_NAME}")
    message(STATUS "System Processor: ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
    message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
    if(CMAKE_SYSROOT)
        message(STATUS "Sysroot: ${CMAKE_SYSROOT}")
    endif()
    message(STATUS "Find Root Path: ${CMAKE_FIND_ROOT_PATH}")
    message(STATUS "==========================================")
endfunction()

# =============================================================================
# Common Cross-Compilation Settings
# =============================================================================

# Set common cross-compilation flags
set(CMAKE_CROSSCOMPILING TRUE)

# Ensure we don't accidentally use host tools
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Common compiler flags for cross-compilation
set(CMAKE_C_FLAGS_INIT "-fPIC")
set(CMAKE_CXX_FLAGS_INIT "-fPIC")

message(STATUS "Common toolchain utilities loaded")