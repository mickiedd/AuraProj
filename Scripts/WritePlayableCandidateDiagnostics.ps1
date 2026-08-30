[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$RunId,
    [Parameter(Mandatory = $true)][string]$BuildRevision,
    [Parameter(Mandatory = $true)][string]$SessionId,
    [Parameter(Mandatory = $true)][string]$ServerInstanceId,
    [Parameter(Mandatory = $true)][string]$WorldPersistenceId,
    [Parameter(Mandatory = $true)][string]$Map,
    [Parameter(Mandatory = $true)][ValidateSet('Listen','Dedicated','Standalone')][string]$ServerMode,
    [Parameter(Mandatory = $true)][string]$ProviderType,
    [Parameter(Mandatory = $true)][string]$ExternalIdentity,
    [Parameter(Mandatory = $true)][string]$RoleId,
    [Parameter(Mandatory = $true)][string]$ResultCode,
    [Parameter(Mandatory = $true)][string]$Message,
    [string]$ActionCorrelationId = '',
    [UInt64]$RequestId = 0,
    [string]$HmacKeyPath = '',
    [string]$OutputPath = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($OutputPath)) { $OutputPath = Join-Path $repoRoot "Saved/Reports/PlayableCandidate/working/$RunId/diagnostics.json" }

$secret = $null
if (-not [string]::IsNullOrWhiteSpace($HmacKeyPath)) {
    if (-not (Test-Path -LiteralPath $HmacKeyPath -PathType Leaf)) { throw "HMAC key file does not exist: $HmacKeyPath" }
    $secret = [IO.File]::ReadAllBytes((Resolve-Path -LiteralPath $HmacKeyPath).Path)
}
elseif (-not [string]::IsNullOrWhiteSpace($env:AURA_CANDIDATE_HMAC_KEY_BASE64)) {
    try { $secret = [Convert]::FromBase64String($env:AURA_CANDIDATE_HMAC_KEY_BASE64) }
    catch { throw 'AURA_CANDIDATE_HMAC_KEY_BASE64 is not valid base64.' }
}
else {
    throw 'A run-scoped HMAC key must be supplied through -HmacKeyPath or AURA_CANDIDATE_HMAC_KEY_BASE64.'
}
if ($secret.Length -lt 32) { [Array]::Clear($secret, 0, $secret.Length); throw 'The run-scoped HMAC key must contain at least 32 bytes.' }

$hmac = [Security.Cryptography.HMACSHA256]::new($secret)
try {
    $identityBytes = [Text.Encoding]::UTF8.GetBytes("$ProviderType`:$ExternalIdentity")
    $identityHash = ([BitConverter]::ToString($hmac.ComputeHash($identityBytes))).Replace('-', '').ToLowerInvariant()
}
finally {
    $hmac.Dispose()
    [Array]::Clear($secret, 0, $secret.Length)
}

$record = [ordered]@{
    schemaVersion = 2
    timestampUtc = (Get-Date).ToUniversalTime().ToString('o')
    buildRevision = $BuildRevision
    runId = $RunId
    sessionId = $SessionId
    serverInstanceId = $ServerInstanceId
    worldPersistenceId = $WorldPersistenceId
    map = $Map
    serverMode = $ServerMode
    providerType = $ProviderType
    playerIdentityHmac = $identityHash
    identityRepresentation = 'run-scoped-HMAC'
    roleId = $RoleId
    actionCorrelationId = if ([string]::IsNullOrWhiteSpace($ActionCorrelationId)) { $null } else { $ActionCorrelationId }
    requestId = if ($RequestId -eq 0) { $null } else { $RequestId }
    resultCode = $ResultCode
    message = $Message
    rawExternalIdentity = 'REDACTED'
    secret = 'REDACTED'
    crossRunLinkability = $false
}
New-Item -ItemType Directory -Force -Path (Split-Path $OutputPath) | Out-Null
$record | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $OutputPath -Encoding utf8
Write-Output "Redacted diagnostics written: $OutputPath"
