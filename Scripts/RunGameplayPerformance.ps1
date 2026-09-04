param([Parameter(Mandatory=$true)][string]$EvidencePath)
$ErrorActionPreference = 'Stop'
if (-not (Test-Path -LiteralPath $EvidencePath -PathType Leaf)) { Write-Error 'BLOCKED: explicit performance evidence file is required' }
$e = Get-Content -LiteralPath $EvidencePath -Raw | ConvertFrom-Json
foreach ($field in @('candidateIdentity','baselineIdentity','hardwareFingerprint','settingsFingerprint','traceSha256','rendered')) {
  if ($null -eq $e.$field) { Write-Error "BLOCKED: missing performance evidence field $field" }
}
if (-not $e.rendered -or $e.candidateIdentity -eq $e.baselineIdentity) { Write-Error 'BLOCKED: rendered evidence with distinct identities is required' }
Write-Output 'Evidence shape accepted; run native Day58 validation before claiming PASS.'
