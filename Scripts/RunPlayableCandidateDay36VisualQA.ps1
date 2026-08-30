param([string]$RunId = '', [string]$Profile = '', [ValidateSet('Listen','Dedicated','Both')][string]$Mode = 'Both')
& (Join-Path $PSScriptRoot 'RunPlayableCandidateDay.ps1') -Day 36 -RunId $RunId -Mode $Mode
if ($LASTEXITCODE -eq 1) { exit 1 }
Write-Output "Day 36 visual QA BLOCKED: profile '$Profile' requires packaged captures and cannot pass from token validation."
exit 2
