@echo off
setlocal EnableExtensions EnableDelayedExpansion

REM Build AuraServer dedicated server target.

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "UPROJECT=%SCRIPT_DIR%\Aura.uproject"
if not exist "%UPROJECT%" (
	echo ERROR: Aura.uproject not found at "%UPROJECT%"
	pause
	exit /b 1
)

set "ENGINE_VERSION="
for /f "usebackq tokens=1* delims=: " %%A in (`findstr /r "\"EngineAssociation\"" "%UPROJECT%"`) do (
	set "ENGINE_VERSION=%%~B"
)
set "ENGINE_VERSION=%ENGINE_VERSION:"=%"

if not defined ENGINE_VERSION (
	echo ERROR: Unable to determine EngineVersion from "%UPROJECT%"
	pause
	exit /b 1
)

if defined UE_ENGINE_ROOT (
	set "ENGINE_DIR=%UE_ENGINE_ROOT%"
	if exist "!ENGINE_DIR!\Engine\Build\BatchFiles\Build.bat" goto :found_engine
)

for /f "skip=2 tokens=1*" %%A in ('reg query "HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_VERSION%" /v InstalledDirectory 2^>nul') do (
	set "REG_VALUE=%%B"
)

if not defined REG_VALUE (
	for /f "skip=2 tokens=1,2,*" %%A in ('reg query "HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\%ENGINE_VERSION%" /v InstalledDirectory 2^>nul') do (
		if /i "%%A"=="InstalledDirectory" set "REG_VALUE=%%C"
	)
)

if not defined REG_VALUE (
	for /f "tokens=1,2,*" %%A in ('reg query "HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds" /v "%ENGINE_VERSION%" 2^>nul') do (
		if /i "%%A"=="%ENGINE_VERSION%" set "REG_VALUE=%%C"
	)
)

if defined REG_VALUE (
	set "ENGINE_DIR=%REG_VALUE:"=%"
	if "!ENGINE_DIR:~-1!"=="\" set "ENGINE_DIR=!ENGINE_DIR:~0,-1!"
	if exist "!ENGINE_DIR!\Engine\Build\BatchFiles\Build.bat" goto :found_engine
)

echo ERROR: Unable to find Unreal Engine %ENGINE_VERSION% installation.
echo.
echo Tried:
echo   UE_ENGINE_ROOT environment variable
echo   HKLM\SOFTWARE\EpicGames\Unreal Engine\%ENGINE_VERSION%
echo   HKLM\SOFTWARE\WOW6432Node\EpicGames\Unreal Engine\%ENGINE_VERSION%
echo   HKCU\SOFTWARE\Epic Games\Unreal Engine\Builds
echo.
echo Set UE_ENGINE_ROOT manually, for example:
echo   set "UE_ENGINE_ROOT=D:\UE_5.5"
pause
exit /b 1

:found_engine

set "BUILD_BAT=%UE_ENGINE_ROOT%\Engine\Build\BatchFiles\Build.bat"
if not exist "%BUILD_BAT%" (
	echo ERROR: Build.bat not found at "%BUILD_BAT%"
	pause
	exit /b 1
)

echo Project  : "%UPROJECT%"
echo Engine   : "%ENGINE_VERSION%"
echo Root     : "%UE_ENGINE_ROOT%"
echo.

echo ========================================
echo Building AuraServer (Development)...
echo ========================================
call "%BUILD_BAT%" AuraServer Win64 Development "%UPROJECT%" -waitmutex
if %errorlevel% neq 0 (
	echo BUILD FAILED with exit code %errorlevel%
	pause
	exit /b %errorlevel%
)

echo.
echo Dedicated server build completed.
