@echo off
setlocal enabledelayedexpansion

REM Run Unreal Editor with AuraAbilityGraph smoke test launch parameter
REM Usage: RunSmokeTest.bat
REM This will launch the editor and automatically run the smoke test on startup
REM The console will end after the smoke test is complete, and a unique log file
REM will be saved under Saved\Logs. The wrapper only reports PASS after the
REM supervised process and the matching result log both pass.

echo ========================================
echo Launching Unreal Editor with AuraAbilityGraph smoke test...
echo ========================================
echo.

set "SCRIPT_DIR=%~dp0"
if "%SCRIPT_DIR:~-1%"=="\" set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "PROJECT_FILE=%SCRIPT_DIR%\Aura.uproject"
if not exist "%PROJECT_FILE%" (
	echo ERROR: Aura.uproject not found at "%PROJECT_FILE%"
	exit /b 1
)

set "ENGINE_VERSION="
for /f "usebackq tokens=1* delims=: " %%A in (`findstr /r "\"EngineAssociation\"" "%PROJECT_FILE%"`) do (
	set "ENGINE_VERSION=%%~B"
)
set "ENGINE_VERSION=%ENGINE_VERSION:"=%"

if not defined ENGINE_VERSION (
	echo ERROR: Unable to determine EngineVersion from "%PROJECT_FILE%"
	exit /b 1
)

if defined UE_ENGINE_ROOT (
	set "ENGINE_DIR=%UE_ENGINE_ROOT%"
	if exist "!ENGINE_DIR!\Engine\Binaries\Win64\UnrealEditor.exe" goto :found_engine
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
	if exist "!ENGINE_DIR!\Engine\Binaries\Win64\UnrealEditor.exe" goto :found_engine
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
exit /b 1

:found_engine

set "UNREAL_EDITOR=%ENGINE_DIR%\Engine\Binaries\Win64\UnrealEditor.exe"
if not exist "%UNREAL_EDITOR%" (
	echo ERROR: UnrealEditor.exe not found at "%UNREAL_EDITOR%"
	exit /b 1
)

set "SMOKE_SUPERVISOR=%SCRIPT_DIR%\RunSmokeTestSupervisor.ps1"
if not exist "%SMOKE_SUPERVISOR%" (
	echo ERROR: Smoke-test supervisor not found at "%SMOKE_SUPERVISOR%"
	exit /b 1
)

set "POWERSHELL_EXE=%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe"
if not exist "%POWERSHELL_EXE%" set "POWERSHELL_EXE=powershell.exe"
set "SMOKE_LOG=%SCRIPT_DIR%\Saved\Logs\AuraAbilityGraphSmoke-%RANDOM%-%RANDOM%.log"

echo Project  : "%PROJECT_FILE%"
echo Engine   : "%ENGINE_VERSION%"
echo Editor   : "%UNREAL_EDITOR%"
echo.

"%POWERSHELL_EXE%" -NoProfile -ExecutionPolicy Bypass -File "%SMOKE_SUPERVISOR%" -EditorPath "%UNREAL_EDITOR%" -ProjectPath "%PROJECT_FILE%" -WorkingDirectory "%SCRIPT_DIR%" -LogPath "%SMOKE_LOG%" -TimeoutSeconds 180
set "SMOKE_EXIT=%ERRORLEVEL%"
if not "%SMOKE_EXIT%"=="0" (
	echo.
	echo ========================================
	echo AuraAbilityGraph smoke test FAILED with exit code %SMOKE_EXIT%.
	echo ========================================
	exit /b %SMOKE_EXIT%
)

echo.
echo ========================================
echo AuraAbilityGraph smoke test PASS. Log: "%SMOKE_LOG%"
echo ========================================
exit /b 0
