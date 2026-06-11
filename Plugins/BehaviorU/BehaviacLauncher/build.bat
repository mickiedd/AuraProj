@echo off
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"
cd /d "%SCRIPT_DIR%"

:: -- Step 1: Locate MSBuild
where msbuild >nul 2>&1
if not errorlevel 1 ( set "MSBUILD=msbuild" & goto :restore )
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "!VSWHERE!" ( echo ERROR: vswhere.exe not found. & pause & exit /b 1 )
for /f "usebackq tokens=*" %%p in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%p"
if "!MSBUILD!"=="" ( echo ERROR: Could not locate MSBuild. & pause & exit /b 1 )

:: -- Step 2: Restore WebView2 via dotnet or nuget
:restore
if exist "%SCRIPT_DIR%packages\microsoft.web.webview2" goto :find_ver
if exist "%SCRIPT_DIR%packages\Microsoft.Web.WebView2.*" goto :find_ver_nuget

:: Try dotnet CLI (available with VS 2022)
where dotnet >nul 2>&1
if not errorlevel 1 goto :dotnet_restore

:: Fall back to nuget.exe from PATH or local
where nuget >nul 2>&1
if not errorlevel 1 ( set "NUGET=nuget" & goto :nuget_restore )
if exist "%SCRIPT_DIR%nuget.exe" ( set "NUGET=%SCRIPT_DIR%nuget.exe" & goto :nuget_restore )
echo ERROR: dotnet and nuget.exe not found. Install Visual Studio 2022.
pause
exit /b 1

:dotnet_restore
echo [2/3] Restoring Microsoft.Web.WebView2 via dotnet ...
if not exist "%TEMP%\WV2Restore" mkdir "%TEMP%\WV2Restore"
echo ^<Project Sdk="Microsoft.NET.Sdk"^>^<PropertyGroup^>^<TargetFramework^>net8.0^</TargetFramework^>^</PropertyGroup^>^<ItemGroup^>^<PackageReference Include="Microsoft.Web.WebView2" Version="1.0.2651.64"/^>^</ItemGroup^>^</Project^> > "%TEMP%\WV2Restore\WV2Restore.csproj"
dotnet restore "%TEMP%\WV2Restore\WV2Restore.csproj" --packages "%SCRIPT_DIR%packages" --no-dependencies
if errorlevel 1 ( echo ERROR: dotnet restore failed. & pause & exit /b 1 )
goto :find_ver

:nuget_restore
echo [2/3] Restoring Microsoft.Web.WebView2 via nuget ...
"!NUGET!" install Microsoft.Web.WebView2 -OutputDirectory "%SCRIPT_DIR%packages" -NonInteractive
if errorlevel 1 ( echo ERROR: NuGet restore failed. & pause & exit /b 1 )
goto :find_ver_nuget

:: dotnet puts packages at  packages\microsoft.web.webview2\<ver>\
:find_ver
set "WV2_VERSION="
for /d %%d in ("%SCRIPT_DIR%packages\microsoft.web.webview2\*") do set "WV2_VERSION=%%~nxd"
set "WV2_PKG_BASE=%SCRIPT_DIR%packages\microsoft.web.webview2\!WV2_VERSION!"
if "!WV2_VERSION!"=="" ( echo ERROR: WebView2 version not found. & pause & exit /b 1 )
goto :build

:: nuget puts packages at  packages\Microsoft.Web.WebView2.<ver>\
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

:: -- Step 3: Build
echo [3/3] Building BehaviacLauncher (Release x64) ...
"!MSBUILD!" "%SCRIPT_DIR%BehaviacLauncher.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:WebView2Version=!WV2_VERSION! /p:WebView2PkgBase=!WV2_PKG_BASE! /v:minimal /nologo

if errorlevel 1 (
    echo.
    echo ===================================================
    echo  BUILD FAILED - see errors above
    echo ===================================================
    pause
    exit /b 1
)

echo.
echo ===========================================================================
echo  BUILD SUCCEEDED
echo  Executable: %SCRIPT_DIR%BehaviacLauncher.exe
echo ===========================================================================
echo.
pause
endlocal