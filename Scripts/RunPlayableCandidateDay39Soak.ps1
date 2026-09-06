param(
    [string]$RunId = '',
    [ValidateSet('Listen','Dedicated','Both')][string]$Mode = 'Both',
    [ValidateSet('Aura','Crunch','Both')][string]$Role = 'Both'
)
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& (Join-Path $repoRoot 'RunPlayableCandidate.ps1') -Stage Candidate -Soak -RunId $RunId -Mode $Mode -Role $Role
exit $LASTEXITCODE
