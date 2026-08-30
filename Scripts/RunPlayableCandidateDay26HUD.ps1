param([string]$RunId = '', [ValidateSet('Listen','Dedicated','Both')][string]$Mode = 'Both')
& (Join-Path $PSScriptRoot 'RunPlayableCandidateDay.ps1') -Day 26 -RunId $RunId -Mode $Mode
exit $LASTEXITCODE
