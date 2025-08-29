# =============================================================================
# Hokuyo Hub - Unified Dependency Management Module
# =============================================================================
# This module provides unified dependency resolution for both host and cross-
# compilation builds with consistent policy management.
#
# Phase 3: Dependency Management Unification
# - DEPS_MODE system with auto/system/fetch/bundled modes
# - Per-dependency overrides (DEPS_YAMLCPP, DEPS_URG, DEPS_NNG)
# - Consolidated NNG options under HOKUYO_NNG namespace
# - Backward compatibility shims with deprecation warnings

# Prevent multiple inclusions
if(HOKUYO_DEPENDENCIES_INCLUDED)
    return()
endif()
set(HOKUYO_DEPENDENCIES_INCLUDED TRUE)

include(ExternalProject)
include(FindPkgConfig)

# Ensure Homebrew paths are in CMAKE_PREFIX_PATH for macOS
if(APPLE AND EXISTS "/opt/homebrew")
    list(APPEND CMAKE_PREFIX_PATH "/opt/homebrew")
endif()

# =============================================================================
# Dependency Policy Configuration
# =============================================================================

# Global dependency resolution mode
set(DEPS_MODE "auto" CACHE STRING "Global dependency resolution mode")
set_property(CACHE DEPS_MODE PROPERTY STRINGS "auto" "system" "fetch" "bundled")

# Per-dependency overrides
set(DEPS_YAMLCPP "" CACHE STRING "yaml-cpp dependency mode override (empty=use DEPS_MODE)")
set(DEPS_URG "" CACHE STRING "urg_library dependency mode override (empty=use DEPS_MODE)")
set(DEPS_NNG "" CACHE STRING "NNG dependency mode override (empty=use DEPS_MODE)")
set(DEPS_CROWCPP "" CACHE STRING "CrowCpp dependency mode override (empty=use DEPS_MODE)")

# Unified NNG configuration (replaces ENABLE_NNG/USE_NNG)
option(HOKUYO_NNG_ENABLE "Enable NNG publisher support" ON)

# =============================================================================
# Backward Compatibility Shims
# =============================================================================

# Handle legacy ENABLE_NNG option
if(DEFINED ENABLE_NNG)
    message(DEPRECATION "ENABLE_NNG is deprecated. Use HOKUYO_NNG_ENABLE instead.")
    if(NOT DEFINED HOKUYO_NNG_ENABLE)
        set(HOKUYO_NNG_ENABLE ${ENABLE_NNG} CACHE BOOL "Enable NNG publisher support" FORCE)
    endif()
endif()

# Handle legacy USE_NNG option
if(DEFINED USE_NNG)
    message(DEPRECATION "USE_NNG is deprecated. Use HOKUYO_NNG_ENABLE instead.")
    if(NOT DEFINED HOKUYO_NNG_ENABLE)
        set(HOKUYO_NNG_ENABLE ${USE_NNG} CACHE BOOL "Enable NNG publisher support" FORCE)
    endif()
endif()

# =============================================================================
# Dependency Resolution Functions
# =============================================================================

# Function to resolve dependency mode for a specific dependency
function(resolve_dependency_mode OUTPUT_VAR DEPENDENCY_NAME OVERRIDE_VAR)
    if(${OVERRIDE_VAR})
        set(${OUTPUT_VAR} ${${OVERRIDE_VAR}} PARENT_SCOPE)
        message(STATUS "Using override mode '${${OVERRIDE_VAR}}' for ${DEPENDENCY_NAME}")
    else()
        set(${OUTPUT_VAR} ${DEPS_MODE} PARENT_SCOPE)
        message(STATUS "Using global mode '${DEPS_MODE}' for ${DEPENDENCY_NAME}")
    endif()
endfunction()

# Function to attempt system package discovery (without importing)
function(try_system_package OUTPUT_VAR PACKAGE_NAME)
    set(${OUTPUT_VAR} FALSE PARENT_SCOPE)
    
    if(PACKAGE_NAME STREQUAL "yaml-cpp")
        # Check if yaml-cpp config exists without importing
        find_file(YAMLCPP_CONFIG_FILE yaml-cppConfig.cmake
            PATHS ${CMAKE_PREFIX_PATH}
            PATH_SUFFIXES lib/cmake/yaml-cpp cmake/yaml-cpp
            NO_DEFAULT_PATH
        )
        # Also check for pkgconfig version
        if(PKG_CONFIG_FOUND)
            pkg_check_modules(PC_YAMLCPP QUIET yaml-cpp)
        endif()
        if(YAMLCPP_CONFIG_FILE OR TARGET yaml-cpp::yaml-cpp OR PC_YAMLCPP_FOUND)
            set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
            message(STATUS "Found system yaml-cpp package")
        endif()
    elseif(PACKAGE_NAME STREQUAL "nng")
        # Check if nng config exists or manual search
        find_file(NNG_CONFIG_FILE nngConfig.cmake
            PATHS ${CMAKE_PREFIX_PATH}
            PATH_SUFFIXES lib/cmake/nng cmake/nng
            NO_DEFAULT_PATH
        )
        if(NNG_CONFIG_FILE OR TARGET nng::nng)
            set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
            message(STATUS "Found system nng package")
        else()
            # Fallback to manual search
            find_path(NNG_INCLUDE_DIR nng/nng.h)
            find_library(NNG_LIBRARY nng)
            if(NNG_INCLUDE_DIR AND NNG_LIBRARY)
                set(${OUTPUT_VAR} TRUE PARENT_SCOPE)
                message(STATUS "Found system nng library (manual search)")
            endif()
        endif()
    endif()
endfunction()

# Function to verify architecture compatibility
function(verify_architecture_compatibility)
    message(STATUS "=== Architecture Verification ===")
    message(STATUS "Host system: ${CMAKE_HOST_SYSTEM_NAME} ${CMAKE_HOST_SYSTEM_PROCESSOR}")
    message(STATUS "Target system: ${CMAKE_SYSTEM_NAME} ${CMAKE_SYSTEM_PROCESSOR}")
    message(STATUS "Cross-compiling: ${CMAKE_CROSSCOMPILING}")
    
    # Check for potential Mach-O vs ELF issues
    if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND CMAKE_SYSTEM_NAME STREQUAL "Linux")
        message(STATUS "Cross-compiling from macOS to Linux - ensuring ELF format")
        
        # Verify compiler toolchain
        if(CMAKE_C_COMPILER)
            message(STATUS "C Compiler: ${CMAKE_C_COMPILER}")
        endif()
        if(CMAKE_CXX_COMPILER)
            message(STATUS "C++ Compiler: ${CMAKE_CXX_COMPILER}")
        endif()
        
        # Check for URG library architecture compatibility
        set(URG_LIB_PATH "${CMAKE_SOURCE_DIR}/external/urg_library/current/src/liburg_c.a")
        if(EXISTS "${URG_LIB_PATH}")
            execute_process(
                COMMAND file "${URG_LIB_PATH}"
                OUTPUT_VARIABLE URG_FILE_INFO
                OUTPUT_STRIP_TRAILING_WHITESPACE
            )
            message(STATUS "URG library format: ${URG_FILE_INFO}")
            
            # Check if it's Mach-O format (macOS) when targeting Linux
            if(URG_FILE_INFO MATCHES "Mach-O")
                message(WARNING "URG library is in Mach-O format but targeting Linux ELF. Library will be rebuilt.")
                # Force rebuild by removing the existing library
                file(REMOVE "${URG_LIB_PATH}")
                message(STATUS "Removed incompatible URG library - will rebuild for target architecture")
            elseif(URG_FILE_INFO MATCHES "ELF.*aarch64")
                message(STATUS "URG library is already in correct ELF ARM64 format")
            endif()
        else()
            message(STATUS "URG library not found - will build for target architecture")
        endif()
    endif()
    
    message(STATUS "=================================")
endfunction()

# Function to set up ExternalProject for urg_library
function(setup_urg_external_project)
    set(URG_SRC_DIR ${CMAKE_SOURCE_DIR}/external/urg_library/current)
    
    # Use platform-specific third_party directory based on current build directory
    get_filename_component(BUILD_PLATFORM ${CMAKE_BINARY_DIR} NAME)
    set(URG_INSTALL_DIR ${CMAKE_SOURCE_DIR}/build/${BUILD_PLATFORM}/third_party/urg_library)
    set(URG_INCLUDE_DIR ${URG_INSTALL_DIR}/include)
    set(URG_LIB_DIR ${URG_INSTALL_DIR}/lib)
    
    message(STATUS "URG library will be installed to platform-specific directory: ${URG_INSTALL_DIR}")

    # Create directories
    file(MAKE_DIRECTORY ${URG_INSTALL_DIR})
    file(MAKE_DIRECTORY ${URG_INCLUDE_DIR})
    file(MAKE_DIRECTORY ${URG_LIB_DIR})

    # Determine the appropriate library file name and build output path
    if(WIN32)
        set(URG_LIBRARY_NAME "urg.lib")
        # Fix: VS project outputs to $(SolutionDir)$(Platform)\$(Configuration)\ = vs2019/c/x64/Release/
        set(URG_BUILD_OUTPUT_DIR "${URG_SRC_DIR}/vs2019/c/x64/Release")
        set(URG_BUILD_OUTPUT_LIB "${URG_BUILD_OUTPUT_DIR}/${URG_LIBRARY_NAME}")
        set(URG_FINAL_LIB_PATH "${URG_LIB_DIR}/${URG_LIBRARY_NAME}")
        message(STATUS "URG library build paths:")
        message(STATUS "  Source directory: ${URG_SRC_DIR}")
        message(STATUS "  VS project directory: ${URG_SRC_DIR}/vs2019/c")
        message(STATUS "  Expected output directory: ${URG_BUILD_OUTPUT_DIR}")
        message(STATUS "  Expected library file: ${URG_BUILD_OUTPUT_LIB}")
    else()
        set(URG_LIBRARY_NAME "liburg_c.a")
        set(URG_BUILD_OUTPUT_LIB "${URG_SRC_DIR}/src/${URG_LIBRARY_NAME}")
        set(URG_FINAL_LIB_PATH "${URG_LIB_DIR}/${URG_LIBRARY_NAME}")
    endif()

    # Check if library is already built
    if(EXISTS "${URG_BUILD_OUTPUT_LIB}")
        message(STATUS "Found pre-built urg_library at ${URG_BUILD_OUTPUT_LIB}")
        
        # Copy headers and library directly without building
        file(COPY ${URG_SRC_DIR}/include/c/ DESTINATION ${URG_INCLUDE_DIR})
        file(COPY ${URG_BUILD_OUTPUT_LIB} DESTINATION ${URG_LIB_DIR})
        
        # Create imported target
        add_library(urg_c STATIC IMPORTED GLOBAL)
        set_target_properties(urg_c PROPERTIES
            IMPORTED_LOCATION             ${URG_FINAL_LIB_PATH}
            INTERFACE_INCLUDE_DIRECTORIES ${URG_INCLUDE_DIR}
        )
        
        message(STATUS "urg_library configured using pre-built library")
    else()
        message(STATUS "Pre-built urg_library not found, building from source")
        
        # Configure build command based on platform
        if(WIN32)
            # Use Visual Studio on Windows for MSVC compatibility
            set(URG_VS_PROJECT_DIR "${URG_SRC_DIR}/vs2019/c")
            # Explicitly set the output directory to ensure consistent build location
            set(URG_BUILD_COMMAND
                ${CMAKE_VS_MSBUILD_COMMAND} "${URG_VS_PROJECT_DIR}/urg.sln"
                "/p:Configuration=Release"
                "/p:Platform=x64"
                "/p:PlatformToolset=v142"
                "/t:urg"
                "/verbosity:detailed"
                "/fileLogger"
                "/fileLoggerParameters:LogFile=${URG_VS_PROJECT_DIR}/msbuild.log;Verbosity=detailed"
            )
            message(STATUS "Configuring urg_library for Windows using Visual Studio 2019")
            message(STATUS "Expected build output directory: ${URG_BUILD_OUTPUT_DIR}")
            message(STATUS "MSBuild command: ${URG_BUILD_COMMAND}")
            message(STATUS "=== URG MSBuild Command Debug ===")
            message(STATUS "CMAKE_VS_MSBUILD_COMMAND: ${CMAKE_VS_MSBUILD_COMMAND}")
            message(STATUS "URG_VS_PROJECT_DIR: ${URG_VS_PROJECT_DIR}")
            message(STATUS "Solution file: ${URG_VS_PROJECT_DIR}/urg.sln")
            message(STATUS "Log file will be written to: ${URG_VS_PROJECT_DIR}/msbuild.log")
            
            # Alternative fallback to devenv if msbuild not available
            if(NOT CMAKE_VS_MSBUILD_COMMAND)
                find_program(DEVENV_COMMAND devenv.exe)
                if(DEVENV_COMMAND)
                    set(URG_BUILD_COMMAND
                        ${DEVENV_COMMAND} "${URG_VS_PROJECT_DIR}/urg.sln"
                        /Build "Release|x64"
                        /Project "urg"
                    )
                    message(STATUS "Using devenv.exe for URG library build")
                else()
                    message(WARNING "Neither msbuild nor devenv found. Falling back to make - may cause linking issues.")
                    set(URG_BUILD_COMMAND make -C ${URG_SRC_DIR}/src clean all)
                endif()
            endif()
        elseif(CMAKE_CROSSCOMPILING)
            set(URG_BUILD_COMMAND
                make -C ${URG_SRC_DIR}/src clean all
                CC=${CMAKE_C_COMPILER}
                CXX=${CMAKE_CXX_COMPILER}
                AR=${CMAKE_AR}
                RANLIB=${CMAKE_RANLIB}
            )
            message(STATUS "Configuring urg_library for cross-compilation")
        else()
            set(URG_BUILD_COMMAND make -C ${URG_SRC_DIR}/src clean all)
            message(STATUS "Configuring urg_library for host build")
        endif()

        # ExternalProject configuration
        ExternalProject_Add(urg_library_proj
            SOURCE_DIR        ${URG_SRC_DIR}
            CONFIGURE_COMMAND ""
            BUILD_COMMAND     ${URG_BUILD_COMMAND}
            INSTALL_COMMAND   ""
            BUILD_IN_SOURCE   1
            LOG_BUILD         1
        )

        # Add step to display MSBuild log and verify build output directory after build
        ExternalProject_Add_Step(urg_library_proj verify_build_output
            COMMAND ${CMAKE_COMMAND} -E echo "=== URG Library Build Verification ==="
            COMMAND ${CMAKE_COMMAND} -E echo "Expected output directory: ${URG_BUILD_OUTPUT_DIR}"
            COMMAND ${CMAKE_COMMAND} -E echo "Expected library file: ${URG_BUILD_OUTPUT_LIB}"
            COMMAND ${CMAKE_COMMAND} -E echo "=== MSBuild Log Contents ==="
            COMMAND ${CMAKE_COMMAND} -E echo "Reading MSBuild log from: ${URG_VS_PROJECT_DIR}/msbuild.log"
            COMMAND ${CMAKE_COMMAND} -DLOG_FILE=${URG_VS_PROJECT_DIR}/msbuild.log -P ${CMAKE_SOURCE_DIR}/cmake/display_log.cmake
            COMMAND ${CMAKE_COMMAND} -E echo "=== Build Output Directory Contents ==="
            COMMAND ${CMAKE_COMMAND} -E make_directory ${URG_BUILD_OUTPUT_DIR}
            COMMAND ${CMAKE_COMMAND} -DDIR_TO_LIST=${URG_BUILD_OUTPUT_DIR} -P ${CMAKE_SOURCE_DIR}/cmake/list_directory.cmake
            COMMAND ${CMAKE_COMMAND} -E echo "=== VS Project Output Directories ==="
            COMMAND ${CMAKE_COMMAND} -E echo "Checking default VS project output locations:"
            COMMAND ${CMAKE_COMMAND} -DDIR_TO_LIST=${URG_VS_PROJECT_DIR}/x64/Release -P ${CMAKE_SOURCE_DIR}/cmake/list_directory.cmake
            DEPENDEES build
            ALWAYS 1
        )

        # Add verification step to confirm urg.lib exists before copying
        ExternalProject_Add_Step(urg_library_proj verify_library_exists
            COMMAND ${CMAKE_COMMAND} -E echo "=== URG Library Existence Verification ==="
            COMMAND ${CMAKE_COMMAND} -E echo "Checking for library at: ${URG_BUILD_OUTPUT_LIB}"
            COMMAND ${CMAKE_COMMAND} -E echo "Library exists:"
            COMMAND ${CMAKE_COMMAND} -DFILE_TO_CHECK=${URG_BUILD_OUTPUT_LIB} -P ${CMAKE_SOURCE_DIR}/cmake/check_file_exists.cmake
            DEPENDEES verify_build_output
            ALWAYS 1
        )

        # Copy headers and library
        ExternalProject_Add_Step(urg_library_proj copy_headers
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${URG_SRC_DIR}/include/c ${URG_INCLUDE_DIR}
            DEPENDEES verify_library_exists
            ALWAYS 1
        )

        ExternalProject_Add_Step(urg_library_proj copy_library
            COMMAND ${CMAKE_COMMAND} -E echo "=== URG Library Copy Operation ==="
            COMMAND ${CMAKE_COMMAND} -E echo "Copying from: ${URG_BUILD_OUTPUT_LIB}"
            COMMAND ${CMAKE_COMMAND} -E echo "Copying to: ${URG_FINAL_LIB_PATH}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                    ${URG_BUILD_OUTPUT_LIB} ${URG_FINAL_LIB_PATH}
            COMMAND ${CMAKE_COMMAND} -E echo "Copy operation completed"
            COMMAND ${CMAKE_COMMAND} -E echo "Verifying final library location:"
            COMMAND ${CMAKE_COMMAND} -DFILE_TO_CHECK=${URG_FINAL_LIB_PATH} -P ${CMAKE_SOURCE_DIR}/cmake/check_file_exists.cmake
            DEPENDEES copy_headers
            ALWAYS 1
        )

        # Create imported target
        add_library(urg_c STATIC IMPORTED GLOBAL)
        set_target_properties(urg_c PROPERTIES
            IMPORTED_LOCATION             ${URG_FINAL_LIB_PATH}
            INTERFACE_INCLUDE_DIRECTORIES ${URG_INCLUDE_DIR}
        )
        add_dependencies(urg_c urg_library_proj)
        
        message(STATUS "urg_library ExternalProject configured")
    endif()
    
    # On Windows, link against additional system libraries required by URG
    if(WIN32)
        target_link_libraries(urg_c INTERFACE wsock32 setupapi)
        message(STATUS "Added Windows system libraries (wsock32, setupapi) to urg_c")
    endif()
endfunction()

# Function to set up FetchContent for a dependency
function(setup_fetch_content DEPENDENCY_NAME GIT_REPOSITORY GIT_TAG)
    include(FetchContent)
    
    string(TOLOWER ${DEPENDENCY_NAME} dep_lower)
    FetchContent_Declare(
        ${dep_lower}
        GIT_REPOSITORY ${GIT_REPOSITORY}
        GIT_TAG        ${GIT_TAG}
    )
    
    FetchContent_MakeAvailable(${dep_lower})
    message(STATUS "FetchContent configured for ${DEPENDENCY_NAME}")
endfunction()

# =============================================================================
# Main Dependency Resolution Functions
# =============================================================================

# Resolve yaml-cpp dependency
function(resolve_yamlcpp_dependency)
    # Check if already resolved
    if(TARGET yaml-cpp::yaml-cpp)
        message(STATUS "yaml-cpp already resolved")
        return()
    endif()
    
    resolve_dependency_mode(YAMLCPP_MODE "yaml-cpp" DEPS_YAMLCPP)
    
    if(YAMLCPP_MODE STREQUAL "system")
        find_package(yaml-cpp CONFIG REQUIRED)
        message(STATUS "Using system yaml-cpp package")
    elseif(YAMLCPP_MODE STREQUAL "fetch")
        setup_fetch_content("yaml-cpp"
            "https://github.com/jbeder/yaml-cpp.git"
            "0.8.0")
    elseif(YAMLCPP_MODE STREQUAL "bundled")
        message(FATAL_ERROR "Bundled yaml-cpp not implemented")
    else() # auto mode
        try_system_package(SYSTEM_FOUND "yaml-cpp")
        if(SYSTEM_FOUND)
            find_package(yaml-cpp CONFIG REQUIRED)
            message(STATUS "Auto-selected system yaml-cpp package")
        else()
            setup_fetch_content("yaml-cpp"
                "https://github.com/jbeder/yaml-cpp.git"
                "0.8.0")
            message(STATUS "Auto-selected FetchContent for yaml-cpp")
        endif()
    endif()
endfunction()

# Resolve urg_library dependency
function(resolve_urg_dependency)
    # Check if already resolved
    if(TARGET urg_c)
        message(STATUS "urg_library already resolved")
        return()
    endif()
    
    resolve_dependency_mode(URG_MODE "urg_library" DEPS_URG)
    
    if(URG_MODE STREQUAL "system")
        # Try to find system urg_library (rarely available)
        find_path(URG_INCLUDE_DIR urg_c.h PATH_SUFFIXES urg_c)
        find_library(URG_LIBRARY urg_c)
        if(URG_INCLUDE_DIR AND URG_LIBRARY)
            add_library(urg_c INTERFACE IMPORTED GLOBAL)
            set_target_properties(urg_c PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES ${URG_INCLUDE_DIR}
                INTERFACE_LINK_LIBRARIES ${URG_LIBRARY}
            )
            message(STATUS "Using system urg_library")
        else()
            message(FATAL_ERROR "System urg_library not found")
        endif()
    elseif(URG_MODE STREQUAL "fetch")
        message(STATUS "FetchContent for urg_library not implemented (use bundled instead)")
        setup_urg_external_project()
        message(STATUS "Using bundled urg_library via ExternalProject")
    elseif(URG_MODE STREQUAL "bundled")
        setup_urg_external_project()
        message(STATUS "Using bundled urg_library via ExternalProject")
    else() # auto mode
        # Always use bundled for urg_library (most reliable)
        setup_urg_external_project()
        message(STATUS "Auto-selected bundled urg_library via ExternalProject")
    endif()
endfunction()

# Resolve NNG dependency
function(resolve_nng_dependency)
    if(NOT HOKUYO_NNG_ENABLE)
        message(STATUS "NNG support disabled")
        return()
    endif()
    
    resolve_dependency_mode(NNG_MODE "NNG" DEPS_NNG)
    
    if(NNG_MODE STREQUAL "system")
        try_system_package(SYSTEM_FOUND "nng")
        if(SYSTEM_FOUND)
            if(nng_FOUND)
                # Modern nng with CMake config
                message(STATUS "Using system nng package (CMake config)")
            else()
                # Manual library setup
                add_library(nng INTERFACE IMPORTED GLOBAL)
                set_target_properties(nng PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES ${NNG_INCLUDE_DIR}
                    INTERFACE_LINK_LIBRARIES ${NNG_LIBRARY}
                )
                message(STATUS "Using system nng library (manual)")
            endif()
        else()
            message(FATAL_ERROR "System NNG not found")
        endif()
    elseif(NNG_MODE STREQUAL "fetch")
        setup_fetch_content("nng" 
            "https://github.com/nanomsg/nng.git" 
            "v1.5.2")
    elseif(NNG_MODE STREQUAL "bundled")
        message(FATAL_ERROR "Bundled NNG not implemented")
    else() # auto mode
        try_system_package(SYSTEM_FOUND "nng")
        if(SYSTEM_FOUND)
            if(nng_FOUND)
                message(STATUS "Auto-selected system nng package (CMake config)")
            else()
                add_library(nng INTERFACE IMPORTED GLOBAL)
                set_target_properties(nng PROPERTIES
                    INTERFACE_INCLUDE_DIRECTORIES ${NNG_INCLUDE_DIR}
                    INTERFACE_LINK_LIBRARIES ${NNG_LIBRARY}
                )
                message(STATUS "Auto-selected system nng library (manual)")
            endif()
        else()
            message(WARNING "NNG requested but not found, disabling NNG support")
            set(HOKUYO_NNG_ENABLE OFF CACHE BOOL "Enable NNG publisher support" FORCE)
        endif()
    endif()
endfunction()

# Resolve JsonCpp dependency
function(resolve_jsoncpp_dependency)
    # Check if already resolved
    if(TARGET jsoncpp_lib OR TARGET PkgConfig::jsoncpp)
        message(STATUS "JsonCpp already resolved")
        return()
    endif()
    
    # Try to find system JsonCpp first
    find_package(PkgConfig QUIET)
    if(PKG_CONFIG_FOUND)
        pkg_check_modules(jsoncpp QUIET jsoncpp)
        if(jsoncpp_FOUND)
            # Create an imported target for pkg-config found JsonCpp
            add_library(PkgConfig::jsoncpp INTERFACE IMPORTED GLOBAL)
            set_target_properties(PkgConfig::jsoncpp PROPERTIES
                INTERFACE_INCLUDE_DIRECTORIES "${jsoncpp_INCLUDE_DIRS}"
                INTERFACE_LINK_LIBRARIES "${jsoncpp_LIBRARIES}"
                INTERFACE_LINK_DIRECTORIES "${jsoncpp_LIBRARY_DIRS}"
            )
            message(STATUS "Using system JsonCpp via pkg-config")
            return()
        endif()
    endif()
    
    # Fallback to FetchContent
    include(FetchContent)
    FetchContent_Declare(
        jsoncpp
        GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
        GIT_TAG        1.9.5
        GIT_SHALLOW    TRUE
    )
    
    set(JSONCPP_WITH_TESTS OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_WARNING_AS_ERROR OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_STRICT_ISO OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_PKGCONFIG_SUPPORT OFF CACHE BOOL "" FORCE)
    set(JSONCPP_WITH_CMAKE_PACKAGE OFF CACHE BOOL "" FORCE)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
    set(BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    
    FetchContent_MakeAvailable(jsoncpp)
    message(STATUS "FetchContent configured for JsonCpp")
endfunction()

# Resolve CrowCpp dependency
function(resolve_crowcpp_dependency)
    # Check if already resolved
    if(TARGET Crow::Crow OR TARGET crow)
        message(STATUS "CrowCpp already resolved")
        return()
    endif()
    
    resolve_dependency_mode(CROWCPP_MODE "CrowCpp" DEPS_CROWCPP)
    
    if(CROWCPP_MODE STREQUAL "system")
        # Try to find system CrowCpp package, but don't fail if not found
        find_package(Crow CONFIG QUIET)
        if(Crow_FOUND)
            message(STATUS "Using system CrowCpp package")
        else()
            message(WARNING "System CrowCpp package not found, falling back to FetchContent")
            setup_fetch_content("CrowCpp"
                "https://github.com/CrowCpp/Crow.git"
                "master")
            
            # Configure CrowCpp options after fetch
            set(CROW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(CROW_AMALGAMATE ON CACHE BOOL "" FORCE)
            
            # Create alias target if needed
            if(TARGET crow AND NOT TARGET Crow::Crow)
                add_library(Crow::Crow ALIAS crow)
            endif()
            
            message(STATUS "Using FetchContent CrowCpp master (system fallback)")
        endif()
    elseif(CROWCPP_MODE STREQUAL "fetch")
        setup_fetch_content("CrowCpp"
            "https://github.com/CrowCpp/Crow.git"
            "master")
        
        # Configure CrowCpp options after fetch
        set(CROW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
        set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
        set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
        set(CROW_AMALGAMATE ON CACHE BOOL "" FORCE)
        
        # Create alias target if needed
        if(TARGET crow AND NOT TARGET Crow::Crow)
            add_library(Crow::Crow ALIAS crow)
        endif()
        
        message(STATUS "Using FetchContent CrowCpp master")
    elseif(CROWCPP_MODE STREQUAL "bundled")
        message(FATAL_ERROR "Bundled CrowCpp not implemented")
    else() # auto mode
        find_package(Crow CONFIG QUIET)
        if(Crow_FOUND)
            message(STATUS "Auto-selected system CrowCpp package")
        else()
            setup_fetch_content("CrowCpp"
                "https://github.com/CrowCpp/Crow.git"
                "master")
            
            # Configure CrowCpp options after fetch
            set(CROW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
            set(CROW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
            set(CROW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
            set(CROW_AMALGAMATE ON CACHE BOOL "" FORCE)
            
            # Create alias target if needed
            if(TARGET crow AND NOT TARGET Crow::Crow)
                add_library(Crow::Crow ALIAS crow)
            endif()
            
            message(STATUS "Auto-selected FetchContent for CrowCpp master")
        endif()
    endif()
endfunction()

# =============================================================================
# Main Entry Point
# =============================================================================

function(resolve_all_dependencies)
    message(STATUS "=== Hokuyo Hub Dependency Resolution ===")
    message(STATUS "Global DEPS_MODE: ${DEPS_MODE}")
    message(STATUS "Cross-compiling: ${CMAKE_CROSSCOMPILING}")
    
    # Verify architecture compatibility first
    verify_architecture_compatibility()
    
    # Resolve each dependency
    resolve_yamlcpp_dependency()
    resolve_urg_dependency()
    resolve_nng_dependency()
    resolve_jsoncpp_dependency()
    resolve_crowcpp_dependency()
    
    # Set compile definitions for enabled features
    if(HOKUYO_NNG_ENABLE)
        add_compile_definitions(USE_NNG)
        message(STATUS "NNG support enabled")
    endif()
    
    message(STATUS "========================================")
endfunction()

# =============================================================================
# Utility Functions for Target Configuration
# =============================================================================

# Function to link dependencies to a target
function(link_hokuyo_dependencies TARGET_NAME)
    # Link yaml-cpp (handle different target names)
    if(TARGET yaml-cpp::yaml-cpp)
        target_link_libraries(${TARGET_NAME} PRIVATE yaml-cpp::yaml-cpp)
    elseif(TARGET yaml-cpp)
        target_link_libraries(${TARGET_NAME} PRIVATE yaml-cpp)
    else()
        message(FATAL_ERROR "yaml-cpp target not found")
    endif()
    
    # Link urg_c
    target_link_libraries(${TARGET_NAME} PRIVATE urg_c)
    
    # On Windows, add legacy runtime support for MSVC compatibility
    if(WIN32 AND MSVC)
        # Link legacy runtime libraries to resolve _vsnprintf and similar symbols
        target_link_libraries(${TARGET_NAME} PRIVATE legacy_stdio_definitions)
        message(STATUS "Added legacy MSVC runtime library support for ${TARGET_NAME}")
    endif()
    
    # Link JsonCpp (handle different target names)
    if(TARGET jsoncpp_lib)
        target_link_libraries(${TARGET_NAME} PRIVATE jsoncpp_lib)
    elseif(TARGET PkgConfig::jsoncpp)
        target_link_libraries(${TARGET_NAME} PRIVATE PkgConfig::jsoncpp)
    elseif(TARGET jsoncpp_static)
        target_link_libraries(${TARGET_NAME} PRIVATE jsoncpp_static)
    elseif(TARGET jsoncpp)
        target_link_libraries(${TARGET_NAME} PRIVATE jsoncpp)
    elseif(jsoncpp_FOUND)
        target_include_directories(${TARGET_NAME} PRIVATE ${jsoncpp_INCLUDE_DIRS})
        target_link_libraries(${TARGET_NAME} PRIVATE ${jsoncpp_LIBRARIES})
    else()
        message(FATAL_ERROR "JsonCpp target not found")
    endif()
    
    # Link CrowCpp (handle different target names)
    if(TARGET Crow::Crow)
        target_link_libraries(${TARGET_NAME} PRIVATE Crow::Crow)
    elseif(TARGET crow)
        target_link_libraries(${TARGET_NAME} PRIVATE crow)
    else()
        message(FATAL_ERROR "CrowCpp target not found")
    endif()
    
    # Link NNG if enabled
    if(HOKUYO_NNG_ENABLE)
        if(TARGET nng::nng)
            target_link_libraries(${TARGET_NAME} PRIVATE nng::nng)
        elseif(TARGET nng)
            target_link_libraries(${TARGET_NAME} PRIVATE nng)
        elseif(NNG_LIBRARY)
            target_include_directories(${TARGET_NAME} PRIVATE ${NNG_INCLUDE_DIR})
            target_link_libraries(${TARGET_NAME} PRIVATE ${NNG_LIBRARY})
        endif()
    endif()
endfunction()

message(STATUS "Hokuyo Hub dependency management module loaded")