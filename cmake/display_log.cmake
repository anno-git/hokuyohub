# CMake script to display MSBuild log contents
if(NOT DEFINED LOG_FILE)
    message(FATAL_ERROR "LOG_FILE must be defined")
endif()

if(EXISTS "${LOG_FILE}")
    file(READ "${LOG_FILE}" LOG_CONTENTS)
    message(STATUS "MSBuild Log Contents:")
    message(STATUS "${LOG_CONTENTS}")
else()
    message(STATUS "MSBuild log file not found at: ${LOG_FILE}")
    message(STATUS "This may indicate MSBuild didn't run or failed to create the log")
endif()