[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][ValidateRange(21, 40)][int]$Day,
    [string]$RunId = '',
    [ValidateSet('Listen','Dedicated','Both')][string]$Mode = 'Both'
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($RunId)) { $RunId = "day-$Day-$(Get-Date -Format 'yyyyMMdd-HHmmss')" }
$sourceRevision = (& git -C $repoRoot rev-parse HEAD).Trim()
$artifactRoot = Join-Path $repoRoot "Saved/Reports/PlayableCandidate/$sourceRevision/$RunId"
New-Item -ItemType Directory -Force -Path $artifactRoot | Out-Null
$artifactName = if ($Day -eq 21) { 'day-21-scope.json' } else {
    $scope = Get-Content -LiteralPath (Join-Path $repoRoot 'Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json') -Raw | ConvertFrom-Json
    $contract = @($scope.dayContracts | Where-Object { [int]$_.day -eq $Day })
    if ($contract.Count -eq 1) { [string]$contract[0].artifact } else { "day-$Day.json" }
}
$artifact = Join-Path $artifactRoot $artifactName
$scopeValidator = Join-Path $repoRoot 'Scripts/ValidatePlayableCandidateScope.ps1'
$contentValidator = Join-Path $repoRoot 'Scripts/ValidatePlayableCandidateContent.ps1'
$scopeOutput = (& powershell -NoProfile -ExecutionPolicy Bypass -File $scopeValidator -AsJson | Out-String)
$scopeExit = $LASTEXITCODE
$contentOutput = (& powershell -NoProfile -ExecutionPolicy Bypass -File $contentValidator -AsJson | Out-String)
$contentExit = $LASTEXITCODE
$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if ([string]::IsNullOrWhiteSpace($python)) { $python = (Get-Command py -ErrorAction SilentlyContinue).Source }
if ([string]::IsNullOrWhiteSpace($python)) { throw 'Python 3 is required for the deterministic candidate contract harness.' }
$pythonScript = Join-Path $repoRoot 'Scripts/test_playable_candidate.py'
& $python $pythonScript --days $Day --mode $Mode --output $artifact
$pythonExit = $LASTEXITCODE
if (-not (Test-Path -LiteralPath $artifact)) { @{ schemaVersion = 1; day = $Day; passed = $false; failures = @('candidate harness did not emit an artifact') } | ConvertTo-Json | Set-Content -LiteralPath $artifact -Encoding utf8 }
$report = Get-Content -LiteralPath $artifact -Raw | ConvertFrom-Json
$report | Add-Member -NotePropertyName day -NotePropertyValue $Day -Force
$report | Add-Member -NotePropertyName runId -NotePropertyValue $RunId -Force
$report | Add-Member -NotePropertyName validationMode -NotePropertyValue 'local-contract' -Force
$report | Add-Member -NotePropertyName scopeValidatorExit -NotePropertyValue $scopeExit -Force
$report | Add-Member -NotePropertyName contentValidatorExit -NotePropertyValue $contentExit -Force
$report | Add-Member -NotePropertyName packagedEvidence -NotePropertyValue 'not executed by day contract runner' -Force
$localPassed = $scopeExit -eq 0 -and $contentExit -eq 0 -and $pythonExit -eq 0
$report | Add-Member -NotePropertyName localContractPassed -NotePropertyValue $localPassed -Force
$report | Add-Member -NotePropertyName completionStatus -NotePropertyValue $(if (-not $localPassed) { 'FAIL' } elseif ($Day -eq 21) { 'PASS' } else { 'BLOCKED' }) -Force
$report | Add-Member -NotePropertyName passed -NotePropertyValue ($localPassed -and $Day -eq 21) -Force
$report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $artifact -Encoding utf8
Write-Output "Day $Day local validation: $(if ($localPassed) { 'PASS' } else { 'FAIL' }); completion=$(if ($Day -eq 21 -and $localPassed) { 'PASS' } elseif ($localPassed) { 'BLOCKED' } else { 'FAIL' }) ($artifactName)"
exit $(if (-not $localPassed) { 1 } elseif ($Day -eq 21) { 0 } else { 2 })
