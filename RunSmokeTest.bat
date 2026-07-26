@echo off
REM Run Unreal Editor with AuraAbilityGraph smoke test launch parameter
REM Usage: RunSmokeTest.bat
REM This will launch the editor and automatically run the smoke test on startup

echo ========================================
echo Launching Unreal Editor with AuraAbilityGraph smoke test...
echo ========================================
echo.

"C:\Git\UnrealEngine-5.5\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Git\AuraProj\Aura.uproject" -AuraAbilityGraphSmokeTest -log

echo.
echo ========================================
echo Editor closed. Check C:\Git\AuraProj\Saved\Logs\Aura.log for results.
echo ========================================
