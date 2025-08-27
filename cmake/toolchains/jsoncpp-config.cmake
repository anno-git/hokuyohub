# Minimal jsoncpp configuration for cross-compilation
# This provides a simple jsoncpp target for cross-compilation builds

if(TARGET jsoncpp_lib OR TARGET Jsoncpp_lib)
    message(STATUS "jsoncpp already configured")
    return()
endif()

# Only configure jsoncpp during cross-compilation
if(NOT CMAKE_CROSSCOMPILING)
    return()
endif()

# Defer jsoncpp configuration until after project() is called
function(configure_jsoncpp_for_cross_compilation)
    if(TARGET jsoncpp_lib OR TARGET Jsoncpp_lib)
        return()
    endif()
    
    include(FetchContent)
    
    # Configure jsoncpp with specific options for cross-compilation
    set(JSONCPP_WITH_TESTS OFF CACHE BOOL "Build jsoncpp tests")
    set(JSONCPP_WITH_POST_BUILD_UNITTEST OFF CACHE BOOL "Build jsoncpp post-build tests")
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build shared libraries")
    set(BUILD_OBJECT_LIBS OFF CACHE BOOL "Build object libraries")
    
    FetchContent_Declare(
        jsoncpp
        GIT_REPOSITORY https://github.com/open-source-parsers/jsoncpp.git
        GIT_TAG 1.9.5
    )
    
    FetchContent_MakeAvailable(jsoncpp)
    
    # Create the expected target aliases
    if(TARGET jsoncpp_static)
        add_library(Jsoncpp_lib ALIAS jsoncpp_static)
        set(JSONCPP_INCLUDE_DIRS ${jsoncpp_SOURCE_DIR}/include PARENT_SCOPE)
        set(JSONCPP_LIBRARIES jsoncpp_static PARENT_SCOPE)
        set(JSONCPP_FOUND TRUE PARENT_SCOPE)
        message(STATUS "jsoncpp configured via FetchContent for cross-compilation")
    endif()
endfunction()

# Register the function to be called later
set(CONFIGURE_JSONCPP_FUNCTION "configure_jsoncpp_for_cross_compilation" CACHE INTERNAL "")