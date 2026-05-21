@echo off
setlocal EnableExtensions EnableDelayedExpansion

set "ROOT_DIR=%~dp0"
set "UPROJECT=%ROOT_DIR%Aura.uproject"

if not exist "%UPROJECT%" (
    echo Could not find project file:
    echo   %UPROJECT%
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -Command ^
    "$projectPath = [System.IO.Path]::GetFullPath('%UPROJECT%')" ^
    "; $processes = Get-CimInstance Win32_Process | Where-Object { (($_.Name -ieq 'UnrealEditor.exe' -and $_.CommandLine -and $_.CommandLine -like ('*' + $projectPath + '*') -and $_.CommandLine -like '*-server*') -or ($_.Name -like 'AuraServer*.exe' -and $_.CommandLine -and $_.CommandLine -like ('*' + $projectPath + '*'))) }" ^
    "; if (-not $processes) { Write-Host 'No dedicated server processes found.'; exit 0 }" ^
    "; foreach ($process in $processes) { Write-Host ('Stopping PID ' + $process.ProcessId + ': ' + $process.CommandLine); Stop-Process -Id $process.ProcessId -Force }"

endlocal
exit /b %ERRORLEVEL%