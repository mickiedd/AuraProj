@echo off
REM Build AuraEditor then run the AuraAbilityGraph smoke tests.
REM Generated for the FireBolt LMB fix verification.

echo ========================================
echo [1/2] Building AuraEditor (Development)...
echo ========================================
call "C:\Git\UnrealEngine-5.5\Engine\Build\BatchFiles\Build.bat" AuraEditor Win64 Development -Project="C:\Git\AuraProj\Aura.uproject" -WaitMutex
if %errorlevel% neq 0 (
    echo BUILD FAILED with exit code %errorlevel%
    exit /b %errorlevel%
)

echo.
echo ========================================
echo [2/2] Running AuraAbilityGraph smoke tests (-AuraAbilityGraphSmokeTest)...
echo ========================================
"C:\Git\UnrealEngine-5.5\Engine\Binaries\Win64\UnrealEditor.exe" "C:\Git\AuraProj\Aura.uproject" -AuraAbilityGraphSmokeTest -log
echo.
echo Smoke test run finished. Check Saved\Logs\Aura.log for results.