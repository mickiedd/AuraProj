[CmdletBinding()]
param(
    [string]$ManifestPath = '',
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
$scriptRoot = if ([string]::IsNullOrWhiteSpace($PSScriptRoot)) { (Get-Location).Path } else { $PSScriptRoot }
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $scriptRoot '..\Docs\Plans\Playable-Candidate-Implementation\playable-candidate-scope.json'
}
$root = (Resolve-Path (Join-Path $scriptRoot '..')).Path
$issues = [System.Collections.Generic.List[string]]::new()

function Add-Issue([string]$Message) { [void]$issues.Add($Message) }
function Is-RelativeSafePath([string]$Path) {
    if ([string]::IsNullOrWhiteSpace($Path)) { return $false }
    if ([IO.Path]::IsPathRooted($Path)) { return $false }
    return ($Path -notmatch '(^|[\\/])\.\.([\\/]|$)')
}

$resolvedManifest = Resolve-Path -LiteralPath $ManifestPath -ErrorAction Stop
$manifest = Get-Content -LiteralPath $resolvedManifest -Raw | ConvertFrom-Json
if ($manifest.schemaVersion -ne 1) { Add-Issue 'schemaVersion must be 1' }
if ([string]::IsNullOrWhiteSpace([string]$manifest.scopeRevision)) { Add-Issue 'scopeRevision is required' }
if ([string]::IsNullOrWhiteSpace([string]$manifest.sourceRevisionAtFreeze)) { Add-Issue 'sourceRevisionAtFreeze is required' }
if ([string]$manifest.canonicalMap -ne '/Game/Maps/StartupMap') { Add-Issue 'canonicalMap must resolve to /Game/Maps/StartupMap' }

$expectedLanes = @('Aura-listen','BungeeMan-listen','Aura-dedicated','BungeeMan-dedicated')
$actualLanes = @($manifest.lanes | ForEach-Object { [string]$_.id })
foreach ($lane in $expectedLanes) { if ($actualLanes -notcontains $lane) { Add-Issue "missing mandatory lane: $lane" } }
if (($actualLanes | Sort-Object -Unique).Count -ne $actualLanes.Count) { Add-Issue 'lane IDs must be unique' }
foreach ($lane in @($manifest.lanes)) {
    if (@('Aura','BungeeMan') -notcontains [string]$lane.role) { Add-Issue "unknown lane role: $($lane.id)" }
    if (@('listen','dedicated') -notcontains [string]$lane.topology) { Add-Issue "unknown lane topology: $($lane.id)" }
}

$steps = @($manifest.journeySteps | Sort-Object order)
if ($steps.Count -ne 16) { Add-Issue "journeySteps must contain 16 ordered steps; got $($steps.Count)" }
for ($i = 0; $i -lt $steps.Count; $i++) {
    if ([int]$steps[$i].order -ne ($i + 1)) { Add-Issue "journey step ordering gap at index $i" }
    if ([string]::IsNullOrWhiteSpace([string]$steps[$i].id) -or [string]::IsNullOrWhiteSpace([string]$steps[$i].owner)) { Add-Issue "journey step $i lacks id or owner" }
}

$contracts = @($manifest.dayContracts)
if ($contracts.Count -ne 19) { Add-Issue "dayContracts must cover Days 22–40; got $($contracts.Count)" }
$expectedDays = 22..40
foreach ($day in $expectedDays) {
    $matches = @($contracts | Where-Object { [int]$_.day -eq $day })
    if ($matches.Count -ne 1) { Add-Issue "day $day must have exactly one owner/artifact contract" }
}
$artifacts = @($contracts | ForEach-Object { [string]$_.artifact })
if (($artifacts | Sort-Object -Unique).Count -ne $artifacts.Count) { Add-Issue 'day artifact names must be unique' }

foreach ($path in @($manifest.contentRoots + $manifest.runnerEntryPoints)) {
    if (-not (Is-RelativeSafePath ([string]$path))) { Add-Issue "unsafe relative path: $path" }
}
$allText = $manifest | ConvertTo-Json -Depth 20 -Compress
foreach ($token in @('TBD','choose','normally','if persistent','where applicable')) {
    if ($allText -match [regex]::Escape($token)) { Add-Issue "unresolved decision token: $token" }
}
if ([string]$manifest.fireGunPolicy.fireMode -ne 'SemiAuto') { Add-Issue 'FireGun fireMode must be SemiAuto' }
if ([double]$manifest.fireGunPolicy.minimumShotInterval -le 0) { Add-Issue 'minimumShotInterval must be positive' }
if ([int]$manifest.fireGunPolicy.magazineCapacity -le 0 -or [int]$manifest.fireGunPolicy.reserveCapacity -le 0) { Add-Issue 'FireGun capacities must be positive' }
if ([int]$manifest.rewardPolicy.amount -ne 25 -or [int]$manifest.rewardPolicy.purchasePrice -ne 25) { Add-Issue 'reward and canonical purchase must both be 25' }
if ([int]$manifest.merchantRestockPolicy.initialStock -ne 20 -or [int]$manifest.merchantRestockPolicy.restockIntervalSeconds -ne 600) { Add-Issue 'merchant restock values drifted from frozen contract' }
if ([string]$manifest.externalPreflight.status -notin @('BLOCKED','READY')) { Add-Issue 'external preflight must be BLOCKED or READY' }

$result = [ordered]@{
    schemaVersion = 1
    manifest = (Resolve-Path -LiteralPath $resolvedManifest).Path.Substring($root.Length + 1).Replace('\','/')
    scopeRevision = [string]$manifest.scopeRevision
    laneCount = $actualLanes.Count
    journeyStepCount = $steps.Count
    dayContractCount = $contracts.Count
    issues = @($issues)
    passed = ($issues.Count -eq 0)
}
if ($AsJson) { $result | ConvertTo-Json -Depth 10; exit $(if ($issues.Count -eq 0) { 0 } else { 1 }) }
if ($issues.Count -gt 0) {
    $issues | ForEach-Object { Write-Error $_ }
    exit 1
}
Write-Output "Playable Candidate scope PASS: $($result.scopeRevision), $($result.laneCount) lanes, $($result.dayContractCount) day contracts."
exit 0
