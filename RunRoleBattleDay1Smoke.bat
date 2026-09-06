@echo off
setlocal

REM Bounded Day 1 smoke test: validates the retired-role rejection boundary,
REM forces two authoritative player deaths, checks respawn vitals, and exits.

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
if not defined UE_ENGINE_ROOT (
    echo ERROR: UE_ENGINE_ROOT is not set.
    exit /b 1
)

set "UNREAL_EDITOR=%UE_ENGINE_ROOT%\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UNREAL_EDITOR%" (
    echo ERROR: UnrealEditor.exe not found at "%UNREAL_EDITOR%".
    exit /b 1
)

"%UNREAL_EDITOR%" "%SCRIPT_DIR%\Aura.uproject" -game /Game/Maps/StartupMap -AuraRoleBattleDay1SmokeTest -unattended -nop4 -nullrhi -nosound -log
exit /b %errorlevel%
