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
        set(URG_BUILD_OUTPUT_DIR "${URG_SRC_DIR}/vs2019/c/x64/Release")
        set(URG_BUILD_OUTPUT_LIB "${URG_BUILD_OUTPUT_DIR}/${URG_LIBRARY_NAME}")
        set(URG_FINAL_LIB_PATH "${URG_LIB_DIR}/${URG_LIBRARY_NAME}")
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
            
            # Enhanced Visual Studio build with proper completion waiting
            set(URG_BUILD_COMMAND
                ${CMAKE_COMMAND} -E echo "Starting Visual Studio build..."
            )
            
            if(CMAKE_VS_MSBUILD_COMMAND)
                list(APPEND URG_BUILD_COMMAND
                    COMMAND ${CMAKE_VS_MSBUILD_COMMAND} "${URG_VS_PROJECT_DIR}/urg.sln"
                    "/p:Configuration=Release"
                    "/p:Platform=x64"
                    "/p:PlatformToolset=v142"
                    "/m:1"
                    "/verbosity:minimal"
                )
                message(STATUS "Configuring urg_library for Windows using MSBuild")
            else()
                # Alternative fallback to devenv if msbuild not available
                find_program(DEVENV_COMMAND devenv.exe)
                if(DEVENV_COMMAND)
                    list(APPEND URG_BUILD_COMMAND
                        COMMAND ${DEVENV_COMMAND} "${URG_VS_PROJECT_DIR}/urg.sln"
                        /Build "Release|x64"
                    )
                    message(STATUS "Using devenv.exe for URG library build")
                else()
                    message(WARNING "Neither msbuild nor devenv found. Falling back to make - may cause linking issues.")
                    set(URG_BUILD_COMMAND make -C ${URG_SRC_DIR}/src clean all)
                endif()
            endif()
            
            # Add completion verification
            if(CMAKE_VS_MSBUILD_COMMAND OR DEVENV_COMMAND)
                list(APPEND URG_BUILD_COMMAND
                    COMMAND ${CMAKE_COMMAND} -E echo "Visual Studio build completed"
                    COMMAND ${CMAKE_COMMAND} -E sleep 1
                )
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

        # Copy headers and library with improved Windows compatibility
        ExternalProject_Add_Step(urg_library_proj copy_headers
            COMMAND ${CMAKE_COMMAND} -E copy_directory
                    ${URG_SRC_DIR}/include/c ${URG_INCLUDE_DIR}
            DEPENDEES build
            ALWAYS 1
        )

        if(WIN32)
            # Create script directory and copy script file path
            set(COPY_SCRIPT_PATH "${CMAKE_BINARY_DIR}/copy_urg_windows.cmake")
            
            # Windows-specific copy with retry logic and path normalization
            ExternalProject_Add_Step(urg_library_proj copy_library
                COMMAND ${CMAKE_COMMAND} -E echo "Waiting for Visual Studio build to complete..."
                COMMAND ${CMAKE_COMMAND} -E sleep 2
                COMMAND ${CMAKE_COMMAND} -E echo "Checking for URG library: ${URG_BUILD_OUTPUT_LIB}"
                COMMAND ${CMAKE_COMMAND} -E echo "Target location: ${URG_FINAL_LIB_PATH}"
                COMMAND ${CMAKE_COMMAND} -P "${COPY_SCRIPT_PATH}"
                DEPENDEES build copy_headers
                ALWAYS 1
            )
            
            # Create the Windows copy script with enhanced diagnostics
            file(WRITE "${COPY_SCRIPT_PATH}" "
# Windows URG library copy script with retry logic and diagnostics
set(SOURCE_FILE \"${URG_BUILD_OUTPUT_LIB}\")
set(TARGET_FILE \"${URG_FINAL_LIB_PATH}\")

# Normalize paths for Windows
file(TO_CMAKE_PATH \"\${SOURCE_FILE}\" SOURCE_FILE)
file(TO_CMAKE_PATH \"\${TARGET_FILE}\" TARGET_FILE)

message(STATUS \"=== URG Library Copy Diagnostics ===\")
message(STATUS \"Source file: \${SOURCE_FILE}\")
message(STATUS \"Target file: \${TARGET_FILE}\")

# Check Visual Studio directories
get_filename_component(VS_DIR \"\${SOURCE_FILE}\" DIRECTORY)
message(STATUS \"Visual Studio output directory: \${VS_DIR}\")
if(EXISTS \"\${VS_DIR}\")
    file(GLOB VS_FILES \"\${VS_DIR}/*\")
    message(STATUS \"Files in VS directory: \${VS_FILES}\")
endif()

# Ensure target directory exists
get_filename_component(TARGET_DIR \"\${TARGET_FILE}\" DIRECTORY)
file(MAKE_DIRECTORY \"\${TARGET_DIR}\")
message(STATUS \"Target directory: \${TARGET_DIR}\")

# Retry logic for file copy
set(MAX_RETRIES 8)
set(RETRY_COUNT 0)
set(COPY_SUCCESS FALSE)

while(RETRY_COUNT LESS MAX_RETRIES AND NOT COPY_SUCCESS)
    math(EXPR ATTEMPT_NUM \"\${RETRY_COUNT} + 1\")
    message(STATUS \"=== Copy Attempt \${ATTEMPT_NUM}/\${MAX_RETRIES} ===\")
    
    if(EXISTS \"\${SOURCE_FILE}\")
        # Get file info for diagnostics
        file(SIZE \"\${SOURCE_FILE}\" SOURCE_SIZE)
        message(STATUS \"Found source file (size: \${SOURCE_SIZE} bytes): \${SOURCE_FILE}\")
        
        # Try to copy the file
        execute_process(
            COMMAND \${CMAKE_COMMAND} -E copy_if_different \"\${SOURCE_FILE}\" \"\${TARGET_FILE}\"
            RESULT_VARIABLE COPY_RESULT
            ERROR_VARIABLE COPY_ERROR
            OUTPUT_VARIABLE COPY_OUTPUT
        )
        
        if(COPY_RESULT EQUAL 0)
            # Verify the copy was successful
            if(EXISTS \"\${TARGET_FILE}\")
                file(SIZE \"\${TARGET_FILE}\" TARGET_SIZE)
                if(SOURCE_SIZE EQUAL TARGET_SIZE)
                    message(STATUS \"✅ URG library copied successfully (\${TARGET_SIZE} bytes)\")
                    set(COPY_SUCCESS TRUE)
                else()
                    message(WARNING \"❌ File copied but sizes don't match (source: \${SOURCE_SIZE}, target: \${TARGET_SIZE})\")
                endif()
            else()
                message(WARNING \"❌ Copy command succeeded but target file doesn't exist\")
            endif()
        else()
            message(WARNING \"❌ Copy command failed (exit code: \${COPY_RESULT})\")
            if(COPY_ERROR)
                message(WARNING \"Error: \${COPY_ERROR}\")
            endif()
            if(COPY_OUTPUT)
                message(STATUS \"Output: \${COPY_OUTPUT}\")
            endif()
        endif()
        
        if(NOT COPY_SUCCESS)
            math(EXPR RETRY_COUNT \"\${RETRY_COUNT} + 1\")
            if(RETRY_COUNT LESS MAX_RETRIES)
                set(WAIT_TIME 2)
                math(EXPR WAIT_SCALED \"\${RETRY_COUNT} + \${WAIT_TIME}\")
                message(STATUS \"Waiting \${WAIT_SCALED} seconds before retry...\")
                execute_process(COMMAND \${CMAKE_COMMAND} -E sleep \${WAIT_SCALED})
            endif()
        endif()
    else()
        message(WARNING \"❌ Source file does not exist: \${SOURCE_FILE}\")
        
        # List alternative locations where the file might be
        get_filename_component(SOURCE_DIR \"\${SOURCE_FILE}\" DIRECTORY)
        get_filename_component(VS_ROOT_DIR \"\${SOURCE_DIR}\" DIRECTORY)
        message(STATUS \"Checking alternative locations in \${VS_ROOT_DIR}...\")
        
        file(GLOB_RECURSE FOUND_LIBS \"\${VS_ROOT_DIR}/**/urg.lib\")
        if(FOUND_LIBS)
            message(STATUS \"Found potential URG libraries: \${FOUND_LIBS}\")
        else()
            message(WARNING \"No urg.lib files found in \${VS_ROOT_DIR}\")
        endif()
        
        set(WAIT_TIME 5)
        math(EXPR WAIT_SCALED \"\${RETRY_COUNT} + \${WAIT_TIME}\")
        message(STATUS \"Waiting \${WAIT_SCALED} seconds for build to complete...\")
        execute_process(COMMAND \${CMAKE_COMMAND} -E sleep \${WAIT_SCALED})
        
        math(EXPR RETRY_COUNT \"\${RETRY_COUNT} + 1\")
    endif()
endwhile()

if(NOT COPY_SUCCESS)
    message(STATUS \"=== Final Diagnostics ===\")
    message(STATUS \"Visual Studio build directory listing:\")
    if(EXISTS \"\${VS_DIR}\")
        file(GLOB_RECURSE ALL_FILES \"\${VS_DIR}/**/*\")
        foreach(FILE \${ALL_FILES})
            message(STATUS \"  \${FILE}\")
        endforeach()
    endif()
    message(FATAL_ERROR \"Failed to copy URG library after \${MAX_RETRIES} attempts. Source: \${SOURCE_FILE}, Target: \${TARGET_FILE}\")
else()
    message(STATUS \"=== URG Library Copy Completed Successfully ===\")
endif()
")
        else()
            # Unix/macOS - standard copy
            ExternalProject_Add_Step(urg_library_proj copy_library
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        ${URG_BUILD_OUTPUT_LIB} ${URG_FINAL_LIB_PATH}
                DEPENDEES build copy_headers
                ALWAYS 1
            )
        endif()

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