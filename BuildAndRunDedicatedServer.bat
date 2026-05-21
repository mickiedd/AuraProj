@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT_DIR=%~dp0"
set "UPROJECT=%ROOT_DIR%Aura.uproject"
set "DEFAULT_MAP=/Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon"
set "MAP=%DEFAULT_MAP%"
set "EXTRA_ARGS="
set "SERVER_EXE="
set "RUN_DETACHED=1"

if /i "%~1"=="/?" goto :usage
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="--help" goto :usage

if /i "%~1"=="--foreground" (
    set "RUN_DETACHED=0"
    shift
)

if /i "%~1"=="--detached" (
    set "RUN_DETACHED=1"
    shift
)

if not "%~1"=="" (
    set "MAP=%~1"
    shift
)

:collect_extra_args
if "%~1"=="" goto :after_collect_extra_args

if defined EXTRA_ARGS (
    set "EXTRA_ARGS=%EXTRA_ARGS% %~1"
) else (
    set "EXTRA_ARGS=%~1"
)

shift
goto :collect_extra_args

:after_collect_extra_args
if not exist "%UPROJECT%" (
    echo Could not find project file:
    echo   %UPROJECT%
    exit /b 1
)

call "%ROOT_DIR%BuildDedicatedServer.bat"
if errorlevel 1 (
    exit /b !ERRORLEVEL!
)

if exist "%ROOT_DIR%Binaries\Win64\AuraServer.exe" set "SERVER_EXE=%ROOT_DIR%Binaries\Win64\AuraServer.exe"
if not defined SERVER_EXE if exist "%ROOT_DIR%Binaries\Win64\AuraServer-Win64-Development.exe" set "SERVER_EXE=%ROOT_DIR%Binaries\Win64\AuraServer-Win64-Development.exe"
if not defined SERVER_EXE if exist "%ROOT_DIR%Binaries\Win64\AuraServer-Win64-DebugGame.exe" set "SERVER_EXE=%ROOT_DIR%Binaries\Win64\AuraServer-Win64-DebugGame.exe"

if not defined SERVER_EXE (
    echo Could not find a built dedicated server executable in:
    echo   %ROOT_DIR%Binaries\Win64
    echo.
    echo Expected one of:
    echo   AuraServer.exe
    echo   AuraServer-Win64-Development.exe
    echo   AuraServer-Win64-DebugGame.exe
    exit /b 1
)

echo Launching built dedicated server:
echo   %SERVER_EXE%
echo   Project: %UPROJECT%
echo   Map: %MAP%
if defined EXTRA_ARGS echo   Extra args: %EXTRA_ARGS%
echo.

if "%RUN_DETACHED%"=="1" (
    start "Aura Dedicated Server (Built)" "%SERVER_EXE%" "%MAP%" -server -log %EXTRA_ARGS%

    rem Detect immediate startup crashes so this script doesn't look like a build-only flow.
    timeout /t 3 /nobreak >nul
    tasklist /FI "IMAGENAME eq AuraServer.exe" | find /I "AuraServer.exe" >nul
    if errorlevel 1 (
        echo Dedicated server process exited shortly after launch.
        echo.
        call :print_latest_log_tail
        endlocal
        exit /b 1
    )

    echo Dedicated server started in a new window.
) else (
    "%SERVER_EXE%" "%MAP%" -server -log %EXTRA_ARGS%
    if errorlevel 1 (
        echo Dedicated server exited with code !ERRORLEVEL!.
        echo.
        call :print_latest_log_tail
        endlocal
        exit /b !ERRORLEVEL!
    )
)

endlocal
exit /b 0

:print_latest_log_tail
set "LATEST_LOG_NAME="
for /f "delims=" %%F in ('dir /b /a-d /o-d "%ROOT_DIR%Saved\Logs\Aura*.log" 2^>nul') do (
    set "LATEST_LOG_NAME=%%F"
    goto :log_found
)

echo No Aura log file found in:
echo   %ROOT_DIR%Saved\Logs
goto :eof

:log_found
echo Latest log:
echo   %ROOT_DIR%Saved\Logs\!LATEST_LOG_NAME!
echo.
echo Last 40 log lines:
powershell -NoProfile -Command "Get-Content -Path '%ROOT_DIR%Saved\Logs\!LATEST_LOG_NAME!' -Tail 40"
goto :eof

:usage
echo Usage:
echo   %~nx0 [--foreground ^| --detached] [MapPath] [AdditionalArgs...]
echo.
echo Examples:
echo   %~nx0
echo   %~nx0 --foreground
echo   %~nx0 /Game/Maps/StartupMap -Port=7777
echo   %~nx0 /Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon -Port=7778 -QueryPort=27016
echo.
echo When no MapPath is provided, the script uses:
echo   %DEFAULT_MAP%
echo.
endlocal
exit /b 0
