@echo off
setlocal

REM Aura Game Server Manager - Windows launcher
REM Usage: StartGameServer.bat [--public-host <ip>] [--port <port>] [--server-exe <path>]

set SCRIPT_DIR=%~dp0
set PYTHON_SCRIPT=%SCRIPT_DIR%Scripts\GameServerManager.py

REM Determine public host (defaults to 127.0.0.1 for local; override for LAN/internet)
set PUBLIC_HOST=127.0.0.1
if not "%AURA_PUBLIC_HOST%"=="" set PUBLIC_HOST=%AURA_PUBLIC_HOST%

REM Determine listen port (defaults to 9000; reads gameServerPort from ServerConnection.json at startup)
set GSM_PORT=9000
if not "%AURA_GSM_PORT%"=="" set GSM_PORT=%AURA_GSM_PORT%

REM Server executable override
set SERVER_EXE_ARG=
if not "%AURA_SERVER_EXE%"=="" set SERVER_EXE_ARG=--server-exe "%AURA_SERVER_EXE%"

echo [AuraGSM] Starting Game Server Manager on port %GSM_PORT% (public-host=%PUBLIC_HOST%)...
python "%PYTHON_SCRIPT%" --host 0.0.0.0 --port %GSM_PORT% --public-host %PUBLIC_HOST% %SERVER_EXE_ARG% %*

if %ERRORLEVEL% NEQ 0 (
    echo [AuraGSM] Game Server Manager exited with code %ERRORLEVEL%.
    pause
)

endlocal
