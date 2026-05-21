@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT_DIR=%~dp0"
set "UPROJECT=%ROOT_DIR%Aura.uproject"
set "ENGINE_ASSOC="
set "ENGINE_DIR="
set "BUILD_BAT="

if /i "%~1"=="/?" goto :usage
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="--help" goto :usage

if not exist "%UPROJECT%" (
    echo Could not find project file:
    echo   %UPROJECT%
    exit /b 1
)

if defined UE_ENGINE_ROOT (
    set "BUILD_BAT=%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat"
    if exist "!BUILD_BAT!" goto :found_build
)

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
    set "BUILD_BAT=!ENGINE_DIR!\Engine\Build\BatchFiles\Build.bat"
    if exist "!BUILD_BAT!" goto :found_build
)

echo Could not find Build.bat.
echo.
echo Registry lookup failed for EngineAssociation: !ENGINE_ASSOC!
echo.
echo Set UE_ENGINE_ROOT manually, for example:
echo   set "UE_ENGINE_ROOT=C:\Program Files\Epic Games\UE_5.5"
echo.
exit /b 1

:found_build
echo Project: %UPROJECT%
echo Build:   %BUILD_BAT%
echo Target:  AuraServer Win64 Development
echo.

call "%BUILD_BAT%" AuraServer Win64 Development "%UPROJECT%" -waitmutex
if errorlevel 1 (
    echo.
    echo Dedicated server build failed.
    echo.
    echo Note:
    echo   If the error says "Server targets are not currently supported from this engine distribution",
    echo   you are using a prebuilt Epic Launcher engine.
    echo   Dedicated server targets require a source-built Unreal Engine installation.
    echo.
    echo   Then set:
    echo     set "UE_ENGINE_ROOT=C:\Path\To\Your\UE_SourceBuild"
    exit /b %ERRORLEVEL%
)

echo.
echo Dedicated server build completed.
exit /b 0

:usage
echo Usage:
echo   %~nx0
echo.
echo Optional environment variables:
echo   UE_ENGINE_ROOT  Root folder of Unreal Engine (for example C:\Program Files\Epic Games\UE_5.5)
echo.
endlocal
exit /b 0
