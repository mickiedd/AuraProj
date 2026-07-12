@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM ============================================================================
REM RunClientNullRHI.bat
REM
REM Launches the Aura game client in headless (-nullrhi) editor -game mode and
REM drives the full Login -> Loading -> cross-server travel -> battleground flow
REM automatically, by passing -AutoLoginLevel=<levelId> (consumed by
REM ALoginPlayerController::TryAutoLoginFromCommandLine).
REM
REM Prerequisite: the Game Server Manager (StartGameServer.bat) and a dedicated
REM server for the target level must already be running.  This script launches
REM the client only, matching the flow recorded in Saved/Logs/Aura.log.
REM
REM Usage:
REM   RunClientNullRHI.bat <levelId> [options]
REM
REM Arguments:
REM   <levelId>            Required.  Level id from Content/Config/LevelConfig.json
REM                        (e.g. Scifi_Desert_Level, dungeon_level_1, ...).
REM
REM Options:
REM   -host <ip>           Override the dedicated server / GSM host
REM                        (otherwise read from Content/Config/ServerConnection.json).
REM   -gsmport <port>      Override the Game Server Manager TCP port (default 9000).
REM   -port <port>         Override the LevelConfig fallback dedicated-server port.
REM   -delay <sec>         Auto-login select->connect delay in seconds (default 0.5).
REM   -player <name>       Player name (default: auto-generated NullRHI_<rand>_<rand>,
REM                        so each launch is a distinct player with its own pawn).
REM   -extra "<args>"      Extra Unreal command-line tokens passed through verbatim.
REM   -nolog               Do not pass -log (default includes -log).
REM   -editor-exe <path>   Override UnrealEditor.exe (also via UE_EDITOR_EXE env).
REM   -h  --help           Show this help and list configured level ids.
REM
REM Examples:
REM   RunClientNullRHI.bat Scifi_Desert_Level
REM   RunClientNullRHI.bat dungeon_level_1 -host 127.0.0.1 -gsmport 9000
REM   RunClientNullRHI.bat Scifi_Desert_Level -extra "-FrameRate=30"
REM ============================================================================

set "ROOT_DIR=%~dp0"
set "UPROJECT=%ROOT_DIR%Aura.uproject"
set "LEVEL_CONFIG=%ROOT_DIR%Content\Config\LevelConfig.json"
set "SERVER_CONFIG=%ROOT_DIR%Content\Config\ServerConnection.json"

set "LEVEL_ID="
set "AUTO_HOST="
set "AUTO_GSMPORT="
set "AUTO_PORT="
set "AUTO_DELAY="
set "AUTO_PLAYER="
set "EXTRA_ARGS="
set "USE_LOG=1"
set "UE_EDITOR_EXE=%UE_EDITOR_EXE%"
set "ENGINE_ASSOC="
set "ENGINE_DIR="

if /i "%~1"=="/?" goto :usage
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="--help" goto :usage

if "%~1"=="" goto :usage

REM First positional token is the level id.
set "LEVEL_ID=%~1"
shift

:collect_args
if "%~1"=="" goto :after_collect_args

if /i "%~1"=="-host" (
    set "AUTO_HOST=%~2"
    shift & shift
    goto :collect_args
)
if /i "%~1"=="-gsmport" (
    set "AUTO_GSMPORT=%~2"
    shift & shift
    goto :collect_args
)
if /i "%~1"=="-port" (
    set "AUTO_PORT=%~2"
    shift & shift
    goto :collect_args
)
if /i "%~1"=="-delay" (
    set "AUTO_DELAY=%~2"
    shift & shift
    goto :collect_args
)
if /i "%~1"=="-player" (
    set "AUTO_PLAYER=%~2"
    shift & shift
    goto :collect_args
)
if /i "%~1"=="-extra" (
    set "EXTRA_ARGS=%~2"
    shift & shift
    goto :collect_args
)
if /i "%~1"=="-nolog" (
    set "USE_LOG=0"
    shift
    goto :collect_args
)
if /i "%~1"=="-editor-exe" (
    set "UE_EDITOR_EXE=%~2"
    shift & shift
    goto :collect_args
)

echo Unknown argument: %~1
goto :usage

:after_collect_args

if "%LEVEL_ID%"=="" goto :usage

REM Each nullrhi launch connects as a distinct player so the dedicated server
REM spawns a new pawn per client. Auto-generate a unique name when -player is
REM not given; -player <name> lets you reconnect as a specific existing player.
if "%AUTO_PLAYER%"=="" set "AUTO_PLAYER=NullRHI_%RANDOM%_%RANDOM%"

if not exist "%UPROJECT%" (
    echo Could not find project file:
    echo   %UPROJECT%
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM Resolve UnrealEditor.exe (reuse the engine-resolution pattern from
REM StartDedicatedServer.bat / BuildDedicatedServer.bat).
REM ---------------------------------------------------------------------------
if defined UE_EDITOR_EXE if exist "%UE_EDITOR_EXE%" goto :found

for /f "tokens=2 delims=:," %%A in ('findstr /i /c:"\"EngineAssociation\"" "%UPROJECT%"') do (
    set "ENGINE_ASSOC=%%~A"
)

set "ENGINE_ASSOC=!ENGINE_ASSOC: =!"
set "ENGINE_ASSOC=!ENGINE_ASSOC:\"=!"

if defined ENGINE_ASSOC (
    for /f "tokens=1,2,*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\!ENGINE_ASSOC!" /v InstalledDirectory 2^>nul') do (
        if /i "%%A"=="InstalledDirectory" set "ENGINE_DIR=%%C"
    )

    if not defined ENGINE_DIR (
        for /f "tokens=1,2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\!ENGINE_ASSOC!" /v InstalledDirectory 2^>nul') do (
            if /i "%%A"=="InstalledDirectory" set "ENGINE_DIR=%%C"
        )
    )

    if not defined ENGINE_DIR (
        for /f "tokens=1,2,*" %%A in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v "!ENGINE_ASSOC!" 2^>nul') do (
            if /i "%%A"=="!ENGINE_ASSOC!" set "ENGINE_DIR=%%C"
        )
    )
)

if defined ENGINE_DIR (
    set "UE_EDITOR_EXE=!ENGINE_DIR!\Engine\Binaries\Win64\UnrealEditor.exe"
    if exist "!UE_EDITOR_EXE!" goto :found
)

echo Could not find UnrealEditor.exe.
echo.
echo Registry lookup failed for EngineAssociation: !ENGINE_ASSOC!
echo.
echo Set UE_EDITOR_EXE manually, for example:
echo   set "UE_EDITOR_EXE=C:\Program Files\Epic Games\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe"
echo.
exit /b 1

:found

REM ---------------------------------------------------------------------------
REM Build the Unreal command line.
REM ---------------------------------------------------------------------------
set "CMD_ARGS=%UPROJECT% /Game/Maps/Login -game -nullrhi"
if "%USE_LOG%"=="1" set "CMD_ARGS=%CMD_ARGS% -log"
set "CMD_ARGS=%CMD_ARGS% -AutoLoginLevel=%LEVEL_ID%"
set "CMD_ARGS=%CMD_ARGS% -AutoLoginPlayerName=%AUTO_PLAYER%"
if not "%AUTO_HOST%"==""    set "CMD_ARGS=%CMD_ARGS% -AutoLoginHost=%AUTO_HOST%"
if not "%AUTO_GSMPORT%"=="" set "CMD_ARGS=%CMD_ARGS% -AutoLoginGSMPort=%AUTO_GSMPORT%"
if not "%AUTO_PORT%"==""    set "CMD_ARGS=%CMD_ARGS% -AutoLoginPort=%AUTO_PORT%"
if not "%AUTO_DELAY%"==""   set "CMD_ARGS=%CMD_ARGS% -AutoLoginDelay=%AUTO_DELAY%"
if not "%EXTRA_ARGS%"==""    set "CMD_ARGS=%CMD_ARGS% %EXTRA_ARGS%"

REM Report which server host the client will use (from ServerConnection.json unless overridden).
set "EFFECTIVE_HOST=%AUTO_HOST%"
if "%EFFECTIVE_HOST%"=="" (
    if exist "%SERVER_CONFIG%" (
        for /f "tokens=2 delims=:," %%A in ('findstr /i /c:"\"serverAddress\"" "%SERVER_CONFIG%"') do (
            set "EFFECTIVE_HOST=%%~A"
        )
        set "EFFECTIVE_HOST=!EFFECTIVE_HOST: =!"
        set "EFFECTIVE_HOST=!EFFECTIVE_HOST:\"=!"
    )
)
if "%EFFECTIVE_HOST%"=="" set "EFFECTIVE_HOST=127.0.0.1"

echo Launching Aura nullrhi client:
echo   Editor:  %UE_EDITOR_EXE%
echo   Project: %UPROJECT%
echo   Level:   %LEVEL_ID%
echo   Player:  %AUTO_PLAYER%
echo   Server:  %EFFECTIVE_HOST% (from ServerConnection.json unless -host given)
echo.
echo   Full command:
echo   "%UE_EDITOR_EXE%" %CMD_ARGS%
echo.
echo NOTE: Start the Game Server Manager and a dedicated server for this level
echo       first (e.g. StartGameServer.bat).  Output is written to
echo       %ROOT_DIR%Saved\Logs\Aura.log
echo.

start "Aura NullRHI Client" "%UE_EDITOR_EXE%" %CMD_ARGS%

endlocal
exit /b 0

:usage
echo Usage:
echo   %~nx0 ^<levelId^> [options]
echo.
echo Launches the Aura client in headless -nullrhi editor -game mode and
echo auto-drives Login -> Loading -> cross-server travel -> battleground via
echo -AutoLoginLevel=^<levelId^>.
echo.
echo Options:
echo   -host ^<ip^>          Override dedicated server / GSM host.
echo   -gsmport ^<port^>     Override Game Server Manager port (default 9000).
echo   -port ^<port^>        Override LevelConfig fallback dedicated-server port.
echo   -delay ^<sec^>        Auto-login delay in seconds (default 0.5).
echo   -player ^<name^>      Player name (default: auto NullRHI_^<rand^>_^<rand^>,
echo                       so each launch is a distinct player with its own pawn).
echo   -extra "^<args^>"     Extra Unreal command-line tokens, passed through.
echo   -nolog               Do not pass -log.
echo   -editor-exe ^<path^>  Override UnrealEditor.exe (also UE_EDITOR_EXE env).
echo   -h  --help         Show this help (also accepts /?).
echo.
echo Examples:
echo   %~nx0 Scifi_Desert_Level
echo   %~nx0 dungeon_level_1 -host 127.0.0.1 -gsmport 9000
echo   %~nx0 Scifi_Desert_Level -extra "-FrameRate=30"
echo.
if not exist "%LEVEL_CONFIG%" (
    echo Could not find LevelConfig.json: %LEVEL_CONFIG%
    echo.
    endlocal
    exit /b 0
)
echo Configured level ids in Content\Config\LevelConfig.json:
powershell -NoProfile -Command "$j = Get-Content -Raw '%LEVEL_CONFIG%' | ConvertFrom-Json; $j.levels | ForEach-Object { '  ' + $_.id + '  (' + $_.displayName + ')' }"
echo.
endlocal
exit /b 0
