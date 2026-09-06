[CmdletBinding()]
param(
    [ValidateSet('Fast','Candidate','External')][string]$Stage = 'Fast',
    [ValidateSet('Listen','Dedicated','Both')][string]$Mode = 'Both',
    [ValidateSet('Aura','Crunch','Both')][string]$Role = 'Both',
    [switch]$Soak,
    [switch]$Finalize,
    [string]$RunId = '',
    [string]$Revision = '',
    [string]$DraftPath = '',
    [string]$SoakPath = '',
    [string]$LaneEvidencePath = ''
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path $PSScriptRoot).Path
if ($Soak -and $Stage -ne 'Candidate') { throw '-Soak is valid only with -Stage Candidate.' }
if ($Finalize -and $Stage -ne 'Candidate') { throw '-Finalize is valid only with -Stage Candidate.' }
$missingFinalizeInput = [string]::IsNullOrWhiteSpace($Revision) -or [string]::IsNullOrWhiteSpace($RunId) -or [string]::IsNullOrWhiteSpace($DraftPath) -or [string]::IsNullOrWhiteSpace($SoakPath) -or [string]::IsNullOrWhiteSpace($LaneEvidencePath)
if ($Finalize -and $missingFinalizeInput) {
    throw '-Finalize requires explicit -Revision, -RunId, -DraftPath, -SoakPath, and -LaneEvidencePath.'
}

if ([string]::IsNullOrWhiteSpace($Revision)) { $Revision = (& git -C $repoRoot rev-parse HEAD).Trim() }
$resolvedRevision = (& git -C $repoRoot rev-parse --verify "$Revision^{commit}" 2>$null | Out-String).Trim()
if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($resolvedRevision)) { throw "Revision is not a Git commit: $Revision" }
$Revision = $resolvedRevision
if ([string]::IsNullOrWhiteSpace($RunId)) { $RunId = "candidate-$(Get-Date -Format 'yyyyMMdd-HHmmss')" }

$safeRevision = $Revision -replace '[^A-Za-z0-9._-]', '_'
$safeRunId = $RunId -replace '[^A-Za-z0-9._-]', '_'
$workingRoot = Join-Path $repoRoot "Saved/Reports/PlayableCandidate/$safeRevision/$safeRunId"
New-Item -ItemType Directory -Force -Path $workingRoot | Out-Null

$python = (Get-Command python -ErrorAction SilentlyContinue).Source
if ([string]::IsNullOrWhiteSpace($python)) { $python = (Get-Command py -ErrorAction SilentlyContinue).Source }
if ([string]::IsNullOrWhiteSpace($python)) { throw 'Python 3 is required for the candidate pipeline.' }

$scopePath = Join-Path $repoRoot 'Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json'
$contentManifestPath = Join-Path $repoRoot 'Content/Config/PlayableCandidateManifest.json'
$scope = Get-Content -LiteralPath $scopePath -Raw | ConvertFrom-Json
$scopeRevision = [string]$scope.scopeRevision
$scopeHash = (Get-FileHash -LiteralPath $scopePath -Algorithm SHA256).Hash.ToLowerInvariant()
$contentHash = (Get-FileHash -LiteralPath $contentManifestPath -Algorithm SHA256).Hash.ToLowerInvariant()
$workingTreeStatus = (& git -C $repoRoot status --porcelain | Out-String).Trim()
$currentHead = (& git -C $repoRoot rev-parse HEAD).Trim()
$isFiltered = $Mode -ne 'Both' -or $Role -ne 'Both'

function Invoke-JsonScript([string]$Path) {
    $raw = (& powershell -NoProfile -ExecutionPolicy Bypass -File $Path -AsJson | Out-String)
    $exit = $LASTEXITCODE
    $parsed = try { $raw | ConvertFrom-Json } catch { [pscustomobject]@{ passed = $false; issues = @($raw.Trim()) } }
    [pscustomobject]@{ exitCode = $exit; result = $parsed }
}

function Assert-RecordBinding([object]$Record, [string]$RecordName, [string]$ExpectedPackageHash) {
    if ([string]$Record.runId -ne $RunId) { throw "$RecordName RunId does not match finalization input." }
    if ([string]$Record.sourceRevision -ne $Revision) { throw "$RecordName source revision does not match finalization input." }
    if ([string]$Record.scopeRevision -ne $scopeRevision) { throw "$RecordName scope revision does not match the current scope." }
    if ([string]$Record.scopeManifestSha256 -ne $scopeHash) { throw "$RecordName scope hash does not match the current scope." }
    if ([string]$Record.contentManifestSha256 -ne $contentHash) { throw "$RecordName content hash does not match the current manifest." }
    $packageHash = [string]$Record.packageSha256
    if ($packageHash -notmatch '^[0-9a-fA-F]{64}$') { throw "$RecordName does not contain a valid package SHA-256." }
    if (-not [string]::IsNullOrWhiteSpace($ExpectedPackageHash) -and $packageHash -ne $ExpectedPackageHash) {
        throw "$RecordName package hash does not match the candidate draft."
    }
    return $packageHash.ToLowerInvariant()
}

if ($Finalize) {
    if ($currentHead -ne $Revision) { throw 'Finalization revision must equal the current checked-out HEAD.' }
    if (-not [string]::IsNullOrWhiteSpace($workingTreeStatus)) { throw 'Finalization requires a clean working tree.' }
    foreach ($path in @($DraftPath, $SoakPath, $LaneEvidencePath)) {
        if (-not (Test-Path -LiteralPath $path -PathType Leaf)) { throw "Explicit finalization input does not exist: $path" }
    }

    $candidatePath = Join-Path $workingRoot 'candidate.json'
    if (Test-Path -LiteralPath $candidatePath) { throw "Immutable candidate already exists: $candidatePath" }
    $draft = Get-Content -LiteralPath $DraftPath -Raw | ConvertFrom-Json
    $soakResult = Get-Content -LiteralPath $SoakPath -Raw | ConvertFrom-Json
    $laneEvidence = Get-Content -LiteralPath $LaneEvidencePath -Raw | ConvertFrom-Json
    $packageHash = Assert-RecordBinding $draft 'Draft' ''
    [void](Assert-RecordBinding $soakResult 'Soak result' $packageHash)
    [void](Assert-RecordBinding $laneEvidence 'Lane evidence' $packageHash)

    if ($draft.localDisposition -ne 'PASS' -or $draft.packagedStatus -ne 'PASS') {
        throw 'Draft does not contain passing local and packaged dispositions.'
    }
    if ($soakResult.status -ne 'PASS' -or $soakResult.passed -ne $true -or [int]$soakResult.cyclesPerLane -lt 10) {
        throw 'Soak result does not contain the required passing ten-cycle evidence.'
    }
    $expectedLaneIds = @('Aura-listen','Crunch-listen','Aura-dedicated','Crunch-dedicated')
    $lanes = @($laneEvidence.lanes)
    $actualLaneIds = @($lanes | ForEach-Object { [string]$_.laneId } | Sort-Object)
    $laneSetMismatch = $null -ne (Compare-Object ($expectedLaneIds | Sort-Object) $actualLaneIds)
    if ($lanes.Count -ne 4 -or @($lanes | Where-Object { $_.status -ne 'PASS' }).Count -ne 0 -or $laneSetMismatch) {
        throw 'Finalization requires exactly the four named passing lane records.'
    }
    foreach ($lane in $lanes) {
        if ([string]$lane.packageSha256 -ne $packageHash) { throw "Lane $($lane.laneId) package hash does not match the draft." }
    }

    $scopeResult = Invoke-JsonScript (Join-Path $repoRoot 'Scripts/ValidatePlayableCandidateScope.ps1')
    $contentResult = Invoke-JsonScript (Join-Path $repoRoot 'Scripts/ValidatePlayableCandidateContent.ps1')
    $finalFastArtifact = Join-Path $workingRoot 'final-fast-contract.json'
    & $python (Join-Path $repoRoot 'Scripts/test_playable_candidate.py') --days all --mode Both --output $finalFastArtifact
    $finalFastExit = $LASTEXITCODE
    if ($scopeResult.exitCode -ne 0 -or $contentResult.exitCode -ne 0 -or $finalFastExit -ne 0) {
        throw 'Finalization revalidation failed.'
    }

    $candidate = [ordered]@{
        schemaVersion = 2
        scopeRevision = $scopeRevision
        sourceRevision = $Revision
        runId = $RunId
        status = 'PASS'
        localDisposition = 'PASS'
        externalDisposition = 'BLOCKED'
        scopeManifestSha256 = $scopeHash
        contentManifestSha256 = $contentHash
        packageSha256 = $packageHash
        draftPath = $DraftPath
        soakPath = $SoakPath
        laneEvidencePath = $LaneEvidencePath
        finalFastPath = $finalFastArtifact
        finalizedAtUtc = (Get-Date).ToUniversalTime().ToString('o')
        immutable = $true
    }
    $candidate | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $candidatePath -Encoding utf8
    Set-ItemProperty -LiteralPath $candidatePath -Name IsReadOnly -Value $true
    $humanReport = Join-Path $repoRoot "Docs/Reports/Playable-Candidate-$safeRevision-$safeRunId.md"
    "# Playable Candidate $RunId`n`nSource revision: $Revision`n`nStatus: PASS locally; external preflight remains BLOCKED.`n`nPackage SHA-256: $packageHash`n`nFinal candidate: $candidatePath" | Set-Content -LiteralPath $humanReport -Encoding utf8
    Write-Output "Immutable candidate PASS: $candidatePath"
    exit 0
}

$scopeResult = Invoke-JsonScript (Join-Path $repoRoot 'Scripts/ValidatePlayableCandidateScope.ps1')
$contentResult = Invoke-JsonScript (Join-Path $repoRoot 'Scripts/ValidatePlayableCandidateContent.ps1')
$fastArtifact = Join-Path $workingRoot 'fast-contract.json'
& $python (Join-Path $repoRoot 'Scripts/test_playable_candidate.py') --days all --mode $Mode --output $fastArtifact
$fastExit = $LASTEXITCODE
$fast = Get-Content -LiteralPath $fastArtifact -Raw | ConvertFrom-Json
$localContractStatus = if ($scopeResult.exitCode -eq 0 -and $contentResult.exitCode -eq 0 -and $fastExit -eq 0) { 'PASS' } else { 'FAIL' }
$blockers = [System.Collections.Generic.List[string]]::new()
if ($isFiltered) { [void]$blockers.Add('role/mode filters are diagnostic selectors; only Both/Both can produce PASS') }
if ($Stage -in @('Candidate','External')) {
    [void]$blockers.Add('packaged listen/dedicated build, staged audit, visual evidence, and four-lane execution are not implemented by this runner')
    if (-not [string]::IsNullOrWhiteSpace($workingTreeStatus)) {
        [void]$blockers.Add('candidate publication requires a clean working tree')
    }
}
if ($Stage -eq 'External') {
    [void]$blockers.Add('production provider, App ID, and two authorized stable identities are not provisioned in this checkout')
}

if ($Soak) {
    if ([string]::IsNullOrWhiteSpace($SoakPath)) { $SoakPath = Join-Path $workingRoot 'day-39-soak.json' }
    $localSoakPath = Join-Path $workingRoot 'day-39-local-contract.json'
    & $python (Join-Path $repoRoot 'Scripts/test_playable_candidate.py') --days 39 --mode $Mode --output $localSoakPath
    $localSoakExit = $LASTEXITCODE
    $localSoak = Get-Content -LiteralPath $localSoakPath -Raw | ConvertFrom-Json
    $soakRecord = [ordered]@{
        schemaVersion = 2
        runId = $RunId
        sourceRevision = $Revision
        scopeRevision = $scopeRevision
        scopeManifestSha256 = $scopeHash
        contentManifestSha256 = $contentHash
        packageSha256 = ''
        status = 'BLOCKED'
        passed = $false
        localContractPassed = ($localSoakExit -eq 0 -and $localSoak.passed -eq $true)
        cyclesPerLane = 0
        lanes = @()
        blocker = 'No packaged four-lane ten-cycle soak was executed.'
        localContractPath = $localSoakPath
    }
    $soakRecord | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $SoakPath -Encoding utf8
    [void]$blockers.Add('Day 39 packaged ten-cycle soak evidence is missing')
}

$overallStatus = if ($localContractStatus -eq 'FAIL') { 'FAIL' }
    elseif ($isFiltered) { 'DIAGNOSTIC' }
    elseif ($Stage -eq 'Fast') { 'PASS' }
    else { 'BLOCKED' }
$packagedStatus = if ($Stage -eq 'Fast') { 'NOT_RUN' } else { 'BLOCKED' }
$pipeline = [ordered]@{
    schemaVersion = 2
    runId = $RunId
    sourceRevision = $Revision
    stage = $Stage
    mode = $Mode
    role = $Role
    scopeRevision = $scopeRevision
    status = $overallStatus
    localContractStatus = $localContractStatus
    localDisposition = if ($isFiltered) { 'DIAGNOSTIC' } else { $localContractStatus }
    packagedStatus = $packagedStatus
    externalStatus = if ($Stage -eq 'External') { 'BLOCKED' } else { 'NOT_RUN' }
    blockers = @($blockers)
    scope = $scopeResult.result
    content = $contentResult.result
    fast = $fast
    scopeManifestSha256 = $scopeHash
    contentManifestSha256 = $contentHash
    packageSha256 = ''
    workingTreeStatus = $workingTreeStatus
    workingRoot = $workingRoot.Substring($repoRoot.Length + 1).Replace('\','/')
}
if ([string]::IsNullOrWhiteSpace($DraftPath)) { $DraftPath = Join-Path $workingRoot 'candidate-draft.json' }
$pipeline | ConvertTo-Json -Depth 20 | Set-Content -LiteralPath $DraftPath -Encoding utf8
Write-Output "Playable Candidate stage=$Stage status=$overallStatus local=$localContractStatus packaged=$packagedStatus draft=$DraftPath"
exit $(if ($overallStatus -eq 'PASS') { 0 } elseif ($overallStatus -eq 'FAIL') { 1 } else { 2 })
