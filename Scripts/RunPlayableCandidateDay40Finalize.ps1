param(
    [Parameter(Mandatory = $true)][string]$Revision,
    [Parameter(Mandatory = $true)][string]$RunId,
    [Parameter(Mandatory = $true)][string]$DraftPath,
    [Parameter(Mandatory = $true)][string]$SoakPath,
    [Parameter(Mandatory = $true)][string]$LaneEvidencePath
)
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
& (Join-Path $repoRoot 'RunPlayableCandidate.ps1') -Stage Candidate -Finalize -Revision $Revision -RunId $RunId `
    -DraftPath $DraftPath -SoakPath $SoakPath -LaneEvidencePath $LaneEvidencePath
exit $LASTEXITCODE
