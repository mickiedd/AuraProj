@echo off
setlocal

REM Aura Game Server Manager - Windows launcher
REM Usage: StartGameServer.bat [--public-host <ip>] [--port <port>] [--server-exe <path>] [--persistence-provider <name>]
REM Local default: GSM loopback at 127.0.0.1; ServerConnection.json keeps the advertised serverAddress separate.
REM LAN/public: set AURA_GSM_HOST, AURA_GSM_ADDRESS (client/server manager endpoint), AURA_PUBLIC_HOST,
REM             AURA_GSM_AUTH_TOKEN, AURA_GSM_SERVER_AUTH_TOKEN (optional shared value),
REM             and AURA_GSM_ALLOWED_CLIENTS to one or more client IPs/CIDRs before starting.

set SCRIPT_DIR=%~dp0
set PYTHON_SCRIPT=%SCRIPT_DIR%Scripts\GameServerManager.py

REM Determine public host (defaults to 127.0.0.1 for local; override for LAN/internet)
set PUBLIC_HOST=127.0.0.1
if not "%AURA_PUBLIC_HOST%"=="" set PUBLIC_HOST=%AURA_PUBLIC_HOST%

REM Determine listen port (defaults to 9000; reads gameServerPort from ServerConnection.json at startup)
set GSM_PORT=9000
if not "%AURA_GSM_PORT%"=="" set GSM_PORT=%AURA_GSM_PORT%

REM Bind the control plane to loopback by default; set AURA_GSM_HOST only with an explicit LAN/public allowlist and auth policy.
set GSM_HOST=127.0.0.1
if not "%AURA_GSM_HOST%"=="" set GSM_HOST=%AURA_GSM_HOST%

REM Development identity provider (defaults to NULL for the local WebUI/client harness)
set "PERSISTENCE_PROVIDER=%AURA_PERSISTENCE_PROVIDER%"
if not defined PERSISTENCE_PROVIDER set "PERSISTENCE_PROVIDER=NULL"

REM Server executable override
set SERVER_EXE_ARG=
if not "%AURA_SERVER_EXE%"=="" set SERVER_EXE_ARG=--server-exe "%AURA_SERVER_EXE%"

echo [AuraGSM] Starting Game Server Manager on %GSM_HOST%:%GSM_PORT% (public-host=%PUBLIC_HOST%, persistence-provider=%PERSISTENCE_PROVIDER%)...
python "%PYTHON_SCRIPT%" --host "%GSM_HOST%" --port %GSM_PORT% --public-host %PUBLIC_HOST% --persistence-provider "%PERSISTENCE_PROVIDER%" %SERVER_EXE_ARG% %*

if %ERRORLEVEL% NEQ 0 (
    echo [AuraGSM] Game Server Manager exited with code %ERRORLEVEL%.
    pause
)

endlocal
