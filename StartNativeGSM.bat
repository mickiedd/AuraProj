@echo off
setlocal
set "GSM_EXE=%~dp0Saved\GSMNative\build\Release\AuraGSM.exe"
if not exist "%GSM_EXE%" (
  powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0Scripts\BuildNativeGSM.ps1"
  if errorlevel 1 exit /b 1
)
echo Starting the native GSM dashboard window. Close it to stop GSM and its servers.
echo Use --headless for backend-only operation.
"%GSM_EXE%" --project-root "%~dp0." %*
exit /b %ERRORLEVEL%
