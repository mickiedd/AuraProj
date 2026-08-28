[CmdletBinding()]
param()

# Review smoke suite v2: runs the remaining Day 3-15 network runners.
# Each runner is spawned as an isolated powershell child process so its
# `exit` statement cannot terminate this suite.

$ErrorActionPreference = 'Continue'
Set-Location $PSScriptRoot
$results = @()

function Invoke-Runner {
    param([string]$Name, [string]$Script, [string]$Mode = '')
    Write-Output "===== START $Name $(Get-Date -Format HH:mm:ss) ====="
    if ($Mode) {
        cmd /c "powershell -NoProfile -ExecutionPolicy Bypass -File .\$Script -Mode $Mode" | Out-Null
    }
    else {
        cmd /c "powershell -NoProfile -ExecutionPolicy Bypass -File .\$Script" | Out-Null
    }
    $code = $LASTEXITCODE
    Write-Output "===== END   $Name EXIT=$code ====="
    $results += [pscustomobject]@{ Step = $Name; ExitCode = $code }
}

Invoke-Runner 'Day3-Listen'    'RunRoleBattleDay3NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day3-Dedicated' 'RunRoleBattleDay3NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day4-Listen'    'RunRoleBattleDay4DamageSmoke.ps1'  'Listen'
Invoke-Runner 'Day4-Dedicated' 'RunRoleBattleDay4DamageSmoke.ps1'  'Dedicated'
Invoke-Runner 'Day5-Listen'    'RunRoleBattleDay5ConfigSmoke.ps1'  'Listen'
Invoke-Runner 'Day5-Dedicated' 'RunRoleBattleDay5ConfigSmoke.ps1'  'Dedicated'
Invoke-Runner 'Day6-Listen'    'RunRoleBattleDay6NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day6-Dedicated' 'RunRoleBattleDay6NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day7-Listen'    'RunRoleBattleDay7ListenSmoke.ps1'
Invoke-Runner 'Day7-Dedicated' 'RunRoleBattleDay7DedicatedSmoke.ps1'
Invoke-Runner 'Day8-Listen'    'RunRoleBattleDay8NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day8-Dedicated' 'RunRoleBattleDay8NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day9-Listen'    'RunRoleBattleDay9NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day9-Dedicated' 'RunRoleBattleDay9NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day10-Listen'   'RunRoleBattleDay10NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day10-Dedicated' 'RunRoleBattleDay10NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day11-Listen'   'RunRoleBattleDay11NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day11-Dedicated' 'RunRoleBattleDay11NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day12-Listen'   'RunRoleBattleDay12NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day12-Dedicated' 'RunRoleBattleDay12NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day13-Listen'   'RunRoleBattleDay13NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day13-Dedicated' 'RunRoleBattleDay13NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day14-Listen'   'RunRoleBattleDay14NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day14-Dedicated' 'RunRoleBattleDay14NetworkSmoke.ps1' 'Dedicated'
Invoke-Runner 'Day15-Listen'   'RunRoleBattleDay15NetworkSmoke.ps1' 'Listen'
Invoke-Runner 'Day15-Dedicated' 'RunRoleBattleDay15NetworkSmoke.ps1' 'Dedicated'

Write-Output "===== SUMMARY ====="
$failed = 0
foreach ($r in $results) {
    $status = if ($r.ExitCode -eq 0) { 'PASS' } else { $failed++; 'FAIL' }
    Write-Output ("{0,-18} {1,-5} (exit {2})" -f $r.Step, $status, $r.ExitCode)
}
Write-Output "TOTAL_STEPS=$($results.Count) FAILURES=$failed"
$results | Export-Csv -Path Saved\review_smoke_results2.csv -NoTypeInformation
exit $failed
