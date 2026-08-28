[CmdletBinding()]
param()

# Review smoke suite: runs Day 1 smoke, AuraAbilityGraph smoke, and every
# Day 2-15 network runner sequentially, recording exit codes. Continues on
# failure so the full picture is captured in one pass.

$ErrorActionPreference = 'Continue'
Set-Location $PSScriptRoot
$results = @()

function Invoke-Step {
    param([string]$Name, [scriptblock]$Action)
    Write-Host "===== START $Name $(Get-Date -Format HH:mm:ss) ====="
    & $Action | Out-Null
    $code = $LASTEXITCODE
    Write-Host "===== END   $Name EXIT=$code ====="
    $script:results += [pscustomobject]@{ Step = $Name; ExitCode = $code }
}

# 1. Day 1 runtime smoke
Invoke-Step 'Day1Smoke' { cmd /c ".\RunRoleBattleDay1Smoke.bat" }

# 2. AuraAbilityGraph plugin smoke
Invoke-Step 'AbilityGraphSmoke' { cmd /c ".\RunSmokeTest.bat" }

# 3. Day 2-15 network runners (explicit -Mode everywhere)
Invoke-Step 'Day2-Listen'    { & .\RunRoleBattleDay2NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day2-Dedicated' { & .\RunRoleBattleDay2NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day3-Listen'    { & .\RunRoleBattleDay3NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day3-Dedicated' { & .\RunRoleBattleDay3NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day4-Listen'    { & .\RunRoleBattleDay4DamageSmoke.ps1 -Mode Listen }
Invoke-Step 'Day4-Dedicated' { & .\RunRoleBattleDay4DamageSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day5-Listen'    { & .\RunRoleBattleDay5ConfigSmoke.ps1 -Mode Listen }
Invoke-Step 'Day5-Dedicated' { & .\RunRoleBattleDay5ConfigSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day6-Listen'    { & .\RunRoleBattleDay6NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day6-Dedicated' { & .\RunRoleBattleDay6NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day7-Listen'    { & .\RunRoleBattleDay7ListenSmoke.ps1 }
Invoke-Step 'Day7-Dedicated' { & .\RunRoleBattleDay7DedicatedSmoke.ps1 }
Invoke-Step 'Day8-Listen'    { & .\RunRoleBattleDay8NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day8-Dedicated' { & .\RunRoleBattleDay8NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day9-Listen'    { & .\RunRoleBattleDay9NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day9-Dedicated' { & .\RunRoleBattleDay9NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day10-Listen'   { & .\RunRoleBattleDay10NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day10-Dedicated'{ & .\RunRoleBattleDay10NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day11-Listen'   { & .\RunRoleBattleDay11NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day11-Dedicated'{ & .\RunRoleBattleDay11NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day12-Listen'   { & .\RunRoleBattleDay12NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day12-Dedicated'{ & .\RunRoleBattleDay12NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day13-Listen'   { & .\RunRoleBattleDay13NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day13-Dedicated'{ & .\RunRoleBattleDay13NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day14-Listen'   { & .\RunRoleBattleDay14NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day14-Dedicated'{ & .\RunRoleBattleDay14NetworkSmoke.ps1 -Mode Dedicated }
Invoke-Step 'Day15-Listen'   { & .\RunRoleBattleDay15NetworkSmoke.ps1 -Mode Listen }
Invoke-Step 'Day15-Dedicated'{ & .\RunRoleBattleDay15NetworkSmoke.ps1 -Mode Dedicated }

Write-Host "===== SUMMARY ====="
$failed = 0
foreach ($r in $results) {
    $status = if ($r.ExitCode -eq 0) { 'PASS' } else { $failed++; 'FAIL' }
    Write-Host ("{0,-18} {1,-5} (exit {2})" -f $r.Step, $status, $r.ExitCode)
}
Write-Host "TOTAL_STEPS=$($results.Count) FAILURES=$failed"
$results | Export-Csv -Path Saved\review_smoke_results.csv -NoTypeInformation
exit $failed
