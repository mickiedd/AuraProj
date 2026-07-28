@echo off
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

:: -- Step 1: Locate MSBuild
where msbuild >nul 2>&1
if not errorlevel 1 ( set "MSBUILD=msbuild" & goto :preflight )
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found. & pause & exit /b 1 )
for /f "usebackq tokens=*" %%p in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%p"
if "!MSBUILD!"=="" ( echo ERROR: Could not locate MSBuild. & pause & exit /b 1 )

:preflight
echo [1/4] Checking Windows 10/11 SDK...

set "SDK_OK=0"
if exist "%ProgramFiles(x86)%\Windows Kits\10\Include\10.0.22000.0\um\windows.h" set "SDK_OK=1"
if exist "%ProgramFiles(x86)%\Windows Kits\10\Include\10.0.19041.0\um\windows.h" set "SDK_OK=1"
if exist "%ProgramFiles(x86)%\Windows Kits\10\Include\10.0.20348.0\um\windows.h" set "SDK_OK=1"
if exist "%ProgramFiles(x86)%\Windows Kits\10\Include\10.0.22621.0\um\windows.h" set "SDK_OK=1"

if "%SDK_OK%"=="0" (
    echo.
    echo ===================================================
    echo  ERROR: Windows 10/11 SDK not found.
    echo.
    echo  This launcher requires the Windows SDK to compile.
    echo  Install one of:
    echo    - "Desktop development with C++" workload in VS 2022
    echo    - Or standalone: https://developer.microsoft.com/windows/downloads/windows-sdk/
    echo ===================================================
    pause
    exit /b 1
)

echo [2/4] Windows SDK detected.

:: -- Step 2: Restore WebView2 via dotnet or nuget
echo [3/4] Restoring WebView2 SDK...
if exist "%SCRIPT_DIR%packages\microsoft.web.webview2" goto :find_ver
if exist "%SCRIPT_DIR%packages\Microsoft.Web.WebView2.*" goto :find_ver_nuget

where dotnet >nul 2>&1
if not errorlevel 1 goto :dotnet_restore

where nuget >nul 2>&1
if not errorlevel 1 ( set "NUGET=nuget" & goto :nuget_restore )
if exist "%SCRIPT_DIR%nuget.exe" ( set "NUGET=%SCRIPT_DIR%nuget.exe" & goto :nuget_restore )

echo ERROR: dotnet and nuget.exe not found. Install Visual Studio 2022 or .NET SDK.
pause
exit /b 1

:dotnet_restore
echo   Using dotnet CLI...
if not exist "%TEMP%\WV2Restore" mkdir "%TEMP%\WV2Restore"
echo ^<Project Sdk="Microsoft.NET.Sdk"^>^<PropertyGroup^>^<TargetFramework^>net8.0^</TargetFramework^>^</PropertyGroup^>^<ItemGroup^>^<PackageReference Include="Microsoft.Web.WebView2" Version="1.0.2651.64"/^>^</ItemGroup^>^</Project^> > "%TEMP%\WV2Restore\WV2Restore.csproj"
dotnet restore "%TEMP%\WV2Restore\WV2Restore.csproj" --packages "%SCRIPT_DIR%packages" --no-dependencies
if errorlevel 1 ( echo ERROR: dotnet restore failed. & pause & exit /b 1 )
goto :find_ver

:nuget_restore
echo   Using nuget.exe...
"!NUGET!" install Microsoft.Web.WebView2 -OutputDirectory "%SCRIPT_DIR%packages" -NonInteractive
if errorlevel 1 ( echo ERROR: NuGet restore failed. & pause & exit /b 1 )
goto :find_ver_nuget

:find_ver
set "WV2_VERSION="
for /d %%d in ("%SCRIPT_DIR%packages\microsoft.web.webview2\*") do set "WV2_VERSION=%%~nxd"
set "WV2_PKG_BASE=%SCRIPT_DIR%packages\microsoft.web.webview2\!WV2_VERSION!"
if "!WV2_VERSION!"=="" ( echo ERROR: WebView2 version not found. & pause & exit /b 1 )
goto :build

:find_ver_nuget
set "WV2_VERSION="
for /d %%d in ("%SCRIPT_DIR%packages\Microsoft.Web.WebView2.*") do (
    set "WV2_LEAF=%%~nxd"
    set "WV2_VERSION=!WV2_LEAF:Microsoft.Web.WebView2.=!"
    set "WV2_PKG_BASE=%SCRIPT_DIR%packages\!WV2_LEAF!"
)
if "!WV2_VERSION!"=="" ( echo ERROR: WebView2 version not found. & pause & exit /b 1 )

:build
echo     Found WebView2 version: !WV2_VERSION!
echo     Package base: !WV2_PKG_BASE!

:: -- Step 4: Build (with embedded WebView2)
echo [4/4] Building AuraAbilityGraphLauncher (Release x64) ...
"!MSBUILD!" "%SCRIPT_DIR%AuraAbilityGraphLauncher.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0 /p:WebView2Version=!WV2_VERSION! /p:WebView2PkgBase=!WV2_PKG_BASE! /p:UseWebView2=true /v:minimal /nologo

if errorlevel 1 (
    echo.
    echo ===================================================
    echo  BUILD FAILED - see errors above
    echo ===================================================
    pause
    exit /b 1
)

:: -- Step 5: Ship WebView2Loader.dll next to the produced exe
echo [5/5] Copying WebView2Loader.dll next to AuraAbilityGraphLauncher.exe ...
copy /Y "!WV2_PKG_BASE!\build\native\x64\WebView2Loader.dll" "%SCRIPT_DIR%AuraAbilityGraphLauncher.WebView2Loader.dll" >nul
if errorlevel 1 (
    echo WARNING: Could not copy WebView2Loader.dll. The launcher will fall back
    echo          to opening the editor in the system browser.
)

echo.
echo ===========================================================================
echo  BUILD SUCCEEDED
echo  Executable: %SCRIPT_DIR%AuraAbilityGraphLauncher.exe
echo  Embedding: WebView2 !WV2_VERSION!
echo ===========================================================================
echo.
pause
endlocal
