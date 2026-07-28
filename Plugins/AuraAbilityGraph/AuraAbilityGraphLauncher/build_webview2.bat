@echo off
setlocal EnableDelayedExpansion

set "SCRIPT_DIR=%~dp0"

:: Use staged WebView2 package directly
set "WV2_PKG_BASE=%SCRIPT_DIR%packages\Microsoft.Web.WebView2.1.0.2651.64"
set "WV2_VERSION=1.0.2651.64"

echo [1/2] Building AuraAbilityGraphLauncher with WebView2...
echo     WebView2 package: !WV2_PKG_BASE!

:: Find MSBuild
where msbuild >nul 2>&1
if not errorlevel 1 ( set "MSBUILD=msbuild" & goto :build )
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq tokens=*" %%p in (`"!VSWHERE!" -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe`) do set "MSBUILD=%%p"
if "!MSBUILD!"=="" ( echo ERROR: Could not locate MSBuild. & pause & exit /b 1 )

:build
"!MSBUILD!" "%SCRIPT_DIR%AuraAbilityGraphLauncher.vcxproj" /p:Configuration=Release /p:Platform=x64 /p:WindowsTargetPlatformVersion=10.0 /p:WebView2Version=!WV2_VERSION! /p:WebView2PkgBase="!WV2_PKG_BASE!" /p:UseWebView2=true /v:minimal /nologo

if errorlevel 1 (
    echo.
    echo ===================================================
    echo  BUILD FAILED - see errors above
    echo ===================================================
    pause
    exit /b 1
)

:: Copy WebView2Loader.dll
echo [2/2] Copying WebView2Loader.dll...
copy /Y "!WV2_PKG_BASE!\build\native\x64\WebView2Loader.dll" "%SCRIPT_DIR%AuraAbilityGraphLauncher.WebView2Loader.dll" >nul

echo.
echo ===========================================================================
echo  BUILD SUCCEEDED
echo  Executable: %SCRIPT_DIR%AuraAbilityGraphLauncher.exe
echo ===========================================================================
echo.
pause
endlocal
