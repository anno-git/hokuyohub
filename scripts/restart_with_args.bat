@echo off
setlocal enabledelayedexpansion

:: Windows version of restart script for HokuyoHub
:: This script reads restart arguments from a temporary file and restarts HokuyoHub.exe

set "SCRIPT_NAME=restart_with_args.bat"
set "EXECUTABLE=HokuyoHub.exe"
set "LOG_DIR=logs"
set "LOG_FILE=%LOG_DIR%\restart.log"
set "PID_FILE=hokuyo_hub.pid"
set "DEFAULT_CONFIG=configs\default.yaml"

:: Temporary file candidates (in order of preference)
set "TMP_DIRS=%TEMP% temp tmp"
set "ARG_FILE_NAME=hokuyo_restart_args.txt"

:: Create logs directory if it doesn't exist
if not exist "%LOG_DIR%" mkdir "%LOG_DIR%"

:: Function to log messages with timestamp
goto :main

:log_message
    set "level=%~1"
    set "message=%~2"
    
    :: Get current timestamp
    for /f "tokens=1-4 delims=/ " %%a in ('date /t') do set "current_date=%%c-%%a-%%b"
    for /f "tokens=1-2 delims=: " %%a in ('time /t') do set "current_time=%%a:%%b"
    
    set "log_entry=[%current_date% %current_time%] [%level%] %message%"
    echo !log_entry! >> "%LOG_FILE%"
    echo [%level%] %message%
goto :eof

:log_debug
    set "message=%~1"
    
    :: Get current timestamp
    for /f "tokens=1-4 delims=/ " %%a in ('date /t') do set "current_date=%%c-%%a-%%b"
    for /f "tokens=1-2 delims=: " %%a in ('time /t') do set "current_time=%%a:%%b"
    
    echo [%current_date% %current_time%] [DEBUG] %message% >> "%LOG_FILE%"
goto :eof

:find_args_file
    set "found_file="
    
    for %%d in (%TMP_DIRS%) do (
        if exist "%%d\%ARG_FILE_NAME%" (
            set "found_file=%%d\%ARG_FILE_NAME%"
            call :log_debug "Found arguments file: !found_file!"
            goto :eof
        )
    )
goto :eof

:validate_argument
    set "arg=%~1"
    :: Check for potentially dangerous characters
    echo %arg% | findstr /R "[;&|`$()]" >nul
    if %ERRORLEVEL% equ 0 (
        call :log_message "ERROR" "Potentially dangerous argument detected: %arg%"
        exit /b 1
    )
exit /b 0

:read_args_from_file
    set "args_file=%~1"
    set "args_count=0"
    set "RESTART_ARGS="
    
    if not exist "%args_file%" (
        call :log_message "WARN" "Arguments file not found: %args_file%"
        exit /b 1
    )
    
    call :log_debug "Reading arguments from: %args_file%"
    
    for /f "usebackq delims=" %%a in ("%args_file%") do (
        set "line=%%a"
        
        :: Skip empty lines and comments
        if not "!line!"=="" (
            echo !line! | findstr /R "^[ 	]*#" >nul
            if !ERRORLEVEL! neq 0 (
                :: Validate argument
                call :validate_argument "!line!"
                if !ERRORLEVEL! equ 0 (
                    :: Handle quoted arguments
                    if "!line:~0,1!"=="\"" if "!line:~-1!"=="\"" (
                        set "line=!line:~1,-1!"
                        set "line=!line:\"=\"!"
                    )
                    
                    if !args_count! equ 0 (
                        set "RESTART_ARGS=!line!"
                    ) else (
                        set "RESTART_ARGS=!RESTART_ARGS! !line!"
                    )
                    set /a args_count+=1
                    call :log_debug "Added argument: !line!"
                ) else (
                    exit /b 1
                )
            )
        )
    )
    
    if !args_count! equ 0 (
        call :log_message "WARN" "No valid arguments found in file"
        exit /b 1
    )
    
    call :log_message "INFO" "Successfully read !args_count! arguments from file"
exit /b 0

:stop_existing_process
    set "pid="
    
    if exist "%PID_FILE%" (
        set /p pid=<"%PID_FILE%"
        
        if defined pid (
            :: Check if process is running
            tasklist /FI "PID eq !pid!" 2>nul | find "!pid!" >nul
            if !ERRORLEVEL! equ 0 (
                call :log_message "INFO" "Stopping existing process (PID: !pid!)"
                
                :: Try graceful termination first
                taskkill /PID !pid! >nul 2>&1
                if !ERRORLEVEL! equ 0 (
                    call :log_message "INFO" "Sent termination signal to process !pid!"
                    
                    :: Wait for graceful shutdown (max 10 seconds)
                    set "timeout_count=10"
                    :wait_loop
                    if !timeout_count! gtr 0 (
                        tasklist /FI "PID eq !pid!" 2>nul | find "!pid!" >nul
                        if !ERRORLEVEL! neq 0 (
                            call :log_message "INFO" "Process !pid! terminated gracefully"
                            goto :cleanup_pid
                        )
                        timeout /t 1 /nobreak >nul
                        set /a timeout_count-=1
                        goto :wait_loop
                    )
                    
                    :: Force kill if still running
                    tasklist /FI "PID eq !pid!" 2>nul | find "!pid!" >nul
                    if !ERRORLEVEL! equ 0 (
                        call :log_message "WARN" "Process !pid! did not terminate gracefully, forcing shutdown"
                        taskkill /PID !pid! /F >nul 2>&1
                        timeout /t 1 /nobreak >nul
                    )
                )
            ) else (
                call :log_message "INFO" "PID file exists but process !pid! is not running"
            )
        )
        
        :cleanup_pid
        del "%PID_FILE%" >nul 2>&1
    ) else (
        call :log_message "INFO" "No PID file found, checking for running processes"
        
        :: Check for any running HokuyoHub processes
        for /f "tokens=2" %%p in ('tasklist /FI "IMAGENAME eq %EXECUTABLE%" /FO table /NH 2^>nul') do (
            set "running_pid=%%p"
            if defined running_pid (
                call :log_message "WARN" "Found running HokuyoHub process: !running_pid!"
                call :log_message "INFO" "Stopping orphaned process: !running_pid!"
                taskkill /PID !running_pid! >nul 2>&1
            )
        )
        timeout /t 2 /nobreak >nul
    )
goto :eof

:start_process
    if not exist "%EXECUTABLE%" (
        call :log_message "ERROR" "Executable not found: %EXECUTABLE%"
        exit /b 1
    )
    
    call :log_message "INFO" "Starting HokuyoHub with arguments: %RESTART_ARGS%"
    call :log_debug "Full command: %EXECUTABLE% %RESTART_ARGS%"
    
    :: Start the process and capture output
    if "%RESTART_ARGS%"=="" (
        start /B "" "%EXECUTABLE%" >> "%LOG_FILE%" 2>&1
    ) else (
        start /B "" "%EXECUTABLE%" %RESTART_ARGS% >> "%LOG_FILE%" 2>&1
    )
    
    if %ERRORLEVEL% neq 0 (
        call :log_message "ERROR" "Failed to start process"
        exit /b 1
    )
    
    :: Wait a moment for process to start
    timeout /t 2 /nobreak >nul
    
    :: Verify process started and get PID
    for /f "tokens=2" %%p in ('tasklist /FI "IMAGENAME eq %EXECUTABLE%" /FO table /NH 2^>nul') do (
        set "new_pid=%%p"
        goto :save_pid
    )
    
    call :log_message "ERROR" "Process failed to start or crashed immediately"
    exit /b 1
    
    :save_pid
    if defined new_pid (
        echo !new_pid! > "%PID_FILE%"
        call :log_message "INFO" "Started HokuyoHub with PID: !new_pid!"
        call :log_message "INFO" "Process startup confirmed (PID: !new_pid!)"
        exit /b 0
    ) else (
        call :log_message "ERROR" "Could not retrieve process PID"
        exit /b 1
    )

:cleanup_temp_files
    set "args_file=%~1"
    
    if defined args_file if exist "%args_file%" (
        call :log_message "INFO" "Cleaning up temporary arguments file: %args_file%"
        del "%args_file%" >nul 2>&1
        if exist "%args_file%" (
            call :log_message "WARN" "Failed to remove temporary file: %args_file%"
        )
    )
goto :eof

:main
call :log_message "INFO" "=== HokuyoHub Restart Script Started ==="
call :log_message "INFO" "Script: %SCRIPT_NAME%"
call :log_message "INFO" "Working directory: %CD%"

:: Find arguments file
call :find_args_file
set "args_file=%found_file%"

set "RESTART_ARGS="

if defined args_file (
    call :log_message "INFO" "Found arguments file: %args_file%"
    
    :: Read arguments from file
    call :read_args_from_file "%args_file%"
    if !ERRORLEVEL! equ 0 (
        call :log_message "INFO" "Successfully loaded restart arguments"
    ) else (
        call :log_message "ERROR" "Failed to read arguments from file, using defaults"
        set "RESTART_ARGS="
    )
    
    :: Clean up the temporary file
    call :cleanup_temp_files "%args_file%"
) else (
    call :log_message "INFO" "No arguments file found, using default configuration"
)

:: Use default arguments if none provided
if "%RESTART_ARGS%"=="" (
    if exist "%DEFAULT_CONFIG%" (
        set "RESTART_ARGS=--config %DEFAULT_CONFIG%"
        call :log_message "INFO" "Using default configuration: %DEFAULT_CONFIG%"
    ) else (
        call :log_message "WARN" "Default configuration not found, starting without arguments"
    )
)

:: Stop existing process
call :log_message "INFO" "Stopping existing HokuyoHub process"
call :stop_existing_process

:: Start new process
call :log_message "INFO" "Starting new HokuyoHub process"
call :start_process
if %ERRORLEVEL% equ 0 (
    call :log_message "INFO" "=== HokuyoHub Restart Completed Successfully ==="
    endlocal
    exit /b 0
) else (
    call :log_message "ERROR" "=== HokuyoHub Restart Failed ==="
    endlocal
    exit /b 1
)