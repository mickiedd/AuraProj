@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT_DIR=%~dp0"
set "UPROJECT=%ROOT_DIR%Aura.uproject"
set "DEFAULT_MAP=/Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon"
set "MAP=%DEFAULT_MAP%"
set "EXTRA_ARGS="
set "UE_EDITOR_EXE=%UE_EDITOR_EXE%"
set "ENGINE_ASSOC="
set "ENGINE_DIR="

if /i "%~1"=="/?" goto :usage
if /i "%~1"=="-h" goto :usage
if /i "%~1"=="--help" goto :usage

if not "%~1"=="" (
    set "MAP=%~1"
    shift

    :collect_extra_args
    if "%~1"=="" goto :after_collect_extra_args

    if defined EXTRA_ARGS (
        set "EXTRA_ARGS=%EXTRA_ARGS% %~1"
    ) else (
        set "EXTRA_ARGS=%~1"
    )

    shift
    goto :collect_extra_args
)

:after_collect_extra_args

if not exist "%UPROJECT%" (
    echo Could not find project file:
    echo   %UPROJECT%
    exit /b 1
)

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
echo Launching dedicated server:
echo   %UE_EDITOR_EXE%
echo   Project: %UPROJECT%
echo   Map: %MAP%
if defined EXTRA_ARGS echo   Extra args: %EXTRA_ARGS%
echo.
start "Aura Dedicated Server" "%UE_EDITOR_EXE%" "%UPROJECT%" "%MAP%" %EXTRA_ARGS%

endlocal
exit /b 0

:usage
echo Usage:
echo   %~nx0 [MapPath] [AdditionalArgs...]
echo.
echo Examples:
echo   %~nx0
echo   %~nx0 /Game/Maps/StartupMap -port=7777
echo   %~nx0 /Game/Fantastic_Dungeon_Pack/maps/map_dungeon_level_1_dungeon -port=7778 -QueryPort=27016
echo.
echo When no MapPath is provided, the script uses:
echo   %DEFAULT_MAP%
echo.
endlocal
exit /b 0
