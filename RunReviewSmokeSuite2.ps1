[CmdletBinding()]
param([switch]$ContractTest)

# Review smoke suite v2: runs the remaining Day 3-15 network runners.
# Each runner is spawned as an isolated powershell child process so its
# `exit` statement cannot terminate this suite.

$ErrorActionPreference = 'Continue'
Set-Location $PSScriptRoot
$script:results = [System.Collections.Generic.List[object]]::new()

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
    [void]$script:results.Add([pscustomobject]@{ Step = $Name; ExitCode = $code })
}

$RunnerSpecs = @(
    @('Day3-Listen', 'RunRoleBattleDay3NetworkSmoke.ps1', 'Listen'),
    @('Day3-Dedicated', 'RunRoleBattleDay3NetworkSmoke.ps1', 'Dedicated'),
    @('Day4-Listen', 'RunRoleBattleDay4DamageSmoke.ps1', 'Listen'),
    @('Day4-Dedicated', 'RunRoleBattleDay4DamageSmoke.ps1', 'Dedicated'),
    @('Day5-Listen', 'RunRoleBattleDay5ConfigSmoke.ps1', 'Listen'),
    @('Day5-Dedicated', 'RunRoleBattleDay5ConfigSmoke.ps1', 'Dedicated'),
    @('Day6-Listen', 'RunRoleBattleDay6NetworkSmoke.ps1', 'Listen'),
    @('Day6-Dedicated', 'RunRoleBattleDay6NetworkSmoke.ps1', 'Dedicated'),
    @('Day7-Listen', 'RunRoleBattleDay7ListenSmoke.ps1', ''),
    @('Day7-Dedicated', 'RunRoleBattleDay7DedicatedSmoke.ps1', ''),
    @('Day8-Listen', 'RunRoleBattleDay8NetworkSmoke.ps1', 'Listen'),
    @('Day8-Dedicated', 'RunRoleBattleDay8NetworkSmoke.ps1', 'Dedicated'),
    @('Day9-Listen', 'RunRoleBattleDay9NetworkSmoke.ps1', 'Listen'),
    @('Day9-Dedicated', 'RunRoleBattleDay9NetworkSmoke.ps1', 'Dedicated'),
    @('Day10-Listen', 'RunRoleBattleDay10NetworkSmoke.ps1', 'Listen'),
    @('Day10-Dedicated', 'RunRoleBattleDay10NetworkSmoke.ps1', 'Dedicated'),
    @('Day11-Listen', 'RunRoleBattleDay11NetworkSmoke.ps1', 'Listen'),
    @('Day11-Dedicated', 'RunRoleBattleDay11NetworkSmoke.ps1', 'Dedicated'),
    @('Day12-Listen', 'RunRoleBattleDay12NetworkSmoke.ps1', 'Listen'),
    @('Day12-Dedicated', 'RunRoleBattleDay12NetworkSmoke.ps1', 'Dedicated'),
    @('Day13-Listen', 'RunRoleBattleDay13NetworkSmoke.ps1', 'Listen'),
    @('Day13-Dedicated', 'RunRoleBattleDay13NetworkSmoke.ps1', 'Dedicated'),
    @('Day14-Listen', 'RunRoleBattleDay14NetworkSmoke.ps1', 'Listen'),
    @('Day14-Dedicated', 'RunRoleBattleDay14NetworkSmoke.ps1', 'Dedicated'),
    @('Day15-Listen', 'RunRoleBattleDay15NetworkSmoke.ps1', 'Listen'),
    @('Day15-Dedicated', 'RunRoleBattleDay15NetworkSmoke.ps1', 'Dedicated')
)
if ($ContractTest) {
    $RunnerSpecs = @(
        @('Contract-Pass', 'Scripts/test_review_suite_pass.ps1', ''),
        @('Contract-Fail', 'Scripts/test_review_suite_fail.ps1', '')
    )
}
foreach ($Runner in $RunnerSpecs) {
    Invoke-Runner $Runner[0] $Runner[1] $Runner[2]
}

Write-Output "===== SUMMARY ====="
$failed = 0
foreach ($r in $script:results) {
    $status = if ($r.ExitCode -eq 0) { 'PASS' } else { $failed++; 'FAIL' }
    Write-Output ("{0,-18} {1,-5} (exit {2})" -f $r.Step, $status, $r.ExitCode)
}
Write-Output "TOTAL_STEPS=$($script:results.Count) FAILURES=$failed"
$script:results | Export-Csv -Path Saved\review_smoke_results2.csv -NoTypeInformation
exit $failed
