# CMake script to debug ExternalProject BUILD_COMMAND execution
# Usage: cmake -P cmake/debug_build_command.cmake

message(STATUS "=== ExternalProject BUILD_COMMAND Debug Utility ===")

# Check CMAKE_VS_MSBUILD_COMMAND availability
message(STATUS "")
message(STATUS "1. CMAKE_VS_MSBUILD_COMMAND Verification:")
if(DEFINED CMAKE_VS_MSBUILD_COMMAND)
    message(STATUS "   ✓ CMAKE_VS_MSBUILD_COMMAND is defined: ${CMAKE_VS_MSBUILD_COMMAND}")
    
    if(EXISTS "${CMAKE_VS_MSBUILD_COMMAND}")
        message(STATUS "   ✓ MSBuild executable exists")
        
        # Test MSBuild accessibility
        execute_process(
            COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" /version
            OUTPUT_VARIABLE MSBUILD_VERSION_OUTPUT
            ERROR_VARIABLE MSBUILD_VERSION_ERROR
            RESULT_VARIABLE MSBUILD_VERSION_RESULT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_STRIP_TRAILING_WHITESPACE
        )
        
        if(MSBUILD_VERSION_RESULT EQUAL 0)
            message(STATUS "   ✓ MSBuild is accessible and working")
            message(STATUS "   MSBuild version: ${MSBUILD_VERSION_OUTPUT}")
        else()
            message(STATUS "   ✗ MSBuild execution failed: ${MSBUILD_VERSION_ERROR}")
        endif()
    else()
        message(STATUS "   ✗ MSBuild executable does not exist at specified path")
    endif()
else()
    message(STATUS "   ✗ CMAKE_VS_MSBUILD_COMMAND is not defined")
    
    # Try to find MSBuild manually
    find_program(MSBUILD_FOUND MSBuild.exe
        PATHS 
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/Enterprise/MSBuild/Current/Bin"
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/Professional/MSBuild/Current/Bin" 
            "C:/Program Files (x86)/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin"
            "C:/Program Files/Microsoft Visual Studio/2019/Enterprise/MSBuild/Current/Bin"
            "C:/Program Files/Microsoft Visual Studio/2019/Professional/MSBuild/Current/Bin"
            "C:/Program Files/Microsoft Visual Studio/2019/Community/MSBuild/Current/Bin"
            "C:/Program Files (x86)/MSBuild/14.0/Bin"
        NO_DEFAULT_PATH
    )
    
    if(MSBUILD_FOUND)
        message(STATUS "   → Found MSBuild manually at: ${MSBUILD_FOUND}")
    else()
        message(STATUS "   → MSBuild not found in common locations")
    endif()
endif()

# Check Visual Studio solution file
message(STATUS "")
message(STATUS "2. Visual Studio Project Verification:")
set(URG_SRC_DIR "${CMAKE_SOURCE_DIR}/external/urg_library/current")
set(URG_VS_PROJECT_DIR "${URG_SRC_DIR}/vs2019/c")
set(SOLUTION_FILE "${URG_VS_PROJECT_DIR}/urg.sln")

message(STATUS "   URG VS Project Directory: ${URG_VS_PROJECT_DIR}")
message(STATUS "   Solution File: ${SOLUTION_FILE}")

if(EXISTS "${SOLUTION_FILE}")
    message(STATUS "   ✓ Solution file exists")
    file(SIZE "${SOLUTION_FILE}" SOLUTION_SIZE)
    message(STATUS "   Solution file size: ${SOLUTION_SIZE} bytes")
else()
    message(STATUS "   ✗ Solution file does not exist")
    
    # Check if vs2019 directory exists
    if(EXISTS "${URG_VS_PROJECT_DIR}")
        message(STATUS "   VS2019/c directory exists, listing contents:")
        file(GLOB VS_CONTENTS "${URG_VS_PROJECT_DIR}/*")
        foreach(ITEM IN LISTS VS_CONTENTS)
            get_filename_component(ITEM_NAME ${ITEM} NAME)
            if(IS_DIRECTORY ${ITEM})
                message(STATUS "     [DIR]  ${ITEM_NAME}/")
            else()
                message(STATUS "     [FILE] ${ITEM_NAME}")
            endif()
        endforeach()
    else()
        message(STATUS "   ✗ VS2019/c directory does not exist")
    endif()
endif()

# Check project file
set(PROJECT_FILE "${URG_VS_PROJECT_DIR}/urg.vcxproj")
if(EXISTS "${PROJECT_FILE}")
    message(STATUS "   ✓ Project file exists: urg.vcxproj")
else()
    message(STATUS "   ✗ Project file does not exist: urg.vcxproj")
endif()

# Test build command construction
message(STATUS "")
message(STATUS "3. Build Command Construction Test:")
if(DEFINED CMAKE_VS_MSBUILD_COMMAND AND EXISTS "${SOLUTION_FILE}")
    set(TEST_BUILD_COMMAND
        "${CMAKE_VS_MSBUILD_COMMAND}" "${SOLUTION_FILE}"
        "/p:Configuration=Release"
        "/p:Platform=x64"
        "/p:PlatformToolset=v142"
        "/t:urg"
        "/verbosity:detailed"
        "/fileLogger"
        "/fileLoggerParameters:LogFile=${URG_VS_PROJECT_DIR}/msbuild_test.log;Verbosity=detailed"
    )
    
    message(STATUS "   Constructed build command:")
    message(STATUS "   ${TEST_BUILD_COMMAND}")
    
    # Test if we can at least validate the command syntax
    message(STATUS "   Testing command syntax validation...")
    execute_process(
        COMMAND "${CMAKE_VS_MSBUILD_COMMAND}" "${SOLUTION_FILE}" /help
        OUTPUT_VARIABLE HELP_OUTPUT
        ERROR_VARIABLE HELP_ERROR
        RESULT_VARIABLE HELP_RESULT
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_STRIP_TRAILING_WHITESPACE
    )
    
    if(HELP_RESULT EQUAL 0)
        message(STATUS "   ✓ MSBuild can process the solution file (help command works)")
    else()
        message(STATUS "   ✗ MSBuild failed to process solution file: ${HELP_ERROR}")
    endif()
else()
    message(STATUS "   ✗ Cannot construct build command - missing prerequisites")
endif()

# Check expected output directories
message(STATUS "")
message(STATUS "4. Expected Output Directory Verification:")
set(URG_BUILD_OUTPUT_DIR "${URG_SRC_DIR}/vs2019/c/x64/Release")
set(URG_BUILD_OUTPUT_LIB "${URG_BUILD_OUTPUT_DIR}/urg.lib")

message(STATUS "   Expected output directory: ${URG_BUILD_OUTPUT_DIR}")
message(STATUS "   Expected library file: ${URG_BUILD_OUTPUT_LIB}")

if(EXISTS "${URG_BUILD_OUTPUT_DIR}")
    message(STATUS "   ✓ Output directory exists")
    file(GLOB OUTPUT_CONTENTS "${URG_BUILD_OUTPUT_DIR}/*")
    if(OUTPUT_CONTENTS)
        message(STATUS "   Directory contents:")
        foreach(ITEM IN LISTS OUTPUT_CONTENTS)
            get_filename_component(ITEM_NAME ${ITEM} NAME)
            if(IS_DIRECTORY ${ITEM})
                message(STATUS "     [DIR]  ${ITEM_NAME}/")
            else()
                file(SIZE ${ITEM} ITEM_SIZE)
                message(STATUS "     [FILE] ${ITEM_NAME} (${ITEM_SIZE} bytes)")
            endif()
        endforeach()
    else()
        message(STATUS "   Directory is empty")
    endif()
else()
    message(STATUS "   ✗ Output directory does not exist")
endif()

if(EXISTS "${URG_BUILD_OUTPUT_LIB}")
    file(SIZE "${URG_BUILD_OUTPUT_LIB}" LIB_SIZE)
    message(STATUS "   ✓ Expected library file exists (${LIB_SIZE} bytes)")
else()
    message(STATUS "   ✗ Expected library file does not exist")
endif()

# Check for any existing log files
message(STATUS "")
message(STATUS "5. Log File Investigation:")
file(GLOB LOG_FILES "${URG_VS_PROJECT_DIR}/*.log")
if(LOG_FILES)
    message(STATUS "   Found log files:")
    foreach(LOG_FILE IN LISTS LOG_FILES)
        get_filename_component(LOG_NAME ${LOG_FILE} NAME)
        file(SIZE ${LOG_FILE} LOG_SIZE)
        message(STATUS "     ${LOG_NAME} (${LOG_SIZE} bytes)")
    endforeach()
else()
    message(STATUS "   ✗ No log files found in VS project directory")
endif()

message(STATUS "")
message(STATUS "================================================")
message(STATUS "Diagnostic complete. Review the output above to identify issues.")