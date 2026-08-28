[CmdletBinding()]
param([string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }))

$ErrorActionPreference = 'Continue'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$PowerShellCommand = Get-Command powershell.exe -ErrorAction SilentlyContinue
if ($null -eq $PowerShellCommand) { $PowerShellCommand = Get-Command pwsh.exe -ErrorAction SilentlyContinue }
if ($null -eq $PowerShellCommand) { Write-Error 'A PowerShell executable is required to isolate the Day 7 and Day 6 child runners.'; exit 1 }
$PowerShellExe = $PowerShellCommand.Source

$Day7ReportPath = Join-Path $ProjectRoot 'Saved\Reports\Day7-Listen.json'
$Day7CompositionReportPath = Join-Path $ProjectRoot 'Saved\Reports\Day7-Listen-Composition.json'
$Day6ServerLog = Join-Path $ProjectRoot 'Saved\Logs\Day06-Listen-Server.log'
New-Item -ItemType Directory -Path (Split-Path -Parent $Day7CompositionReportPath) -Force | Out-Null

# These runners end with exit N, so invoke each in a child PowerShell process.
# Always run both sides of the composition so a Day 6 prerequisite failure does
# not get mislabeled as a Day 7-specific assertion failure.
& $PowerShellExe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ProjectRoot 'RunRoleBattleDays789Automation.ps1') -Day 7 -Mode Listen -EngineRoot $EngineRoot | Out-Host
$Day7ExitCode = [int]$LASTEXITCODE

& $PowerShellExe -NoProfile -ExecutionPolicy Bypass -File (Join-Path $ProjectRoot 'RunRoleBattleDay6NetworkSmoke.ps1') -Mode Listen -EngineRoot $EngineRoot | Out-Host
$Day6ExitCode = [int]$LASTEXITCODE

$Day7ReportPassed = $false
if (Test-Path -LiteralPath $Day7ReportPath) {
    try { $Day7ReportPassed = [bool]((Get-Content -Raw -LiteralPath $Day7ReportPath | ConvertFrom-Json).Passed) } catch { $Day7ReportPassed = $false }
}
$Day7SpecificPassed = $Day7ExitCode -eq 0 -and $Day7ReportPassed

$AuraGrant = (Test-Path -LiteralPath $Day6ServerLog) -and [bool](Select-String -LiteralPath $Day6ServerLog -Pattern '\[RoleGrant\]\[Server\] Role=Aura Required=4 OwnedSpecs=4' -Quiet)
$BungeeGrant = (Test-Path -LiteralPath $Day6ServerLog) -and [bool](Select-String -LiteralPath $Day6ServerLog -Pattern '\[RoleGrant\]\[Server\] Role=BungeeMan Required=1 OwnedSpecs=1' -Quiet)
$Day6PrerequisitePassed = $Day6ExitCode -eq 0 -and $AuraGrant -and $BungeeGrant
$AggregatePassed = $Day7SpecificPassed -and $Day6PrerequisitePassed

$Composition = [ordered]@{
    SchemaVersion = 1
    Day = 7
    Mode = 'Listen'
    Day7Specific = [ordered]@{ ExitCode = $Day7ExitCode; ReportPassed = $Day7ReportPassed; Passed = $Day7SpecificPassed; Report = $Day7ReportPath }
    Day6Prerequisite = [ordered]@{ ExitCode = $Day6ExitCode; AuraGrantEvidence = $AuraGrant; BungeeGrantEvidence = $BungeeGrant; Passed = $Day6PrerequisitePassed; ServerLog = $Day6ServerLog }
    AggregatePassed = $AggregatePassed
}
$Composition | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $Day7CompositionReportPath -Encoding UTF8

Write-Host "[RoleBattleDay7][Listen] Day7SpecificStatus=$(if ($Day7SpecificPassed) { 'PASS' } else { 'FAIL' }) (exit $Day7ExitCode)"
Write-Host "[RoleBattleDay7][Listen] Day6PrerequisiteStatus=$(if ($Day6PrerequisitePassed) { 'PASS' } else { 'FAIL' }) (exit $Day6ExitCode)"
if (-not $Day7SpecificPassed) { Write-Error "Day 7 listen-specific runner failed; see '$Day7ReportPath'." }
if (-not $Day6PrerequisitePassed) { Write-Error "Day 6 listen prerequisite failed; see '$Day6ServerLog'." }
if (-not $AggregatePassed) { exit 1 }
Write-Host '[RoleBattleDay7][Listen] PASS'
exit 0
