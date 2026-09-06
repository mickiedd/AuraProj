[CmdletBinding()]
param(
    [string]$ManifestPath = '',
    [string]$ScopePath = '',
    [string]$ContentRoot = '',
    [string]$StagedRoot = '',
    [string]$MalformedFixtureRoot = '',
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
$scriptRoot = if ([string]::IsNullOrWhiteSpace($PSScriptRoot)) { (Get-Location).Path } else { $PSScriptRoot }
$repoRoot = (Resolve-Path (Join-Path $scriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($ManifestPath)) { $ManifestPath = Join-Path $repoRoot 'Content/Config/PlayableCandidateManifest.json' }
if ([string]::IsNullOrWhiteSpace($ScopePath)) { $ScopePath = Join-Path $repoRoot 'Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json' }
if ([string]::IsNullOrWhiteSpace($ContentRoot)) { $ContentRoot = $repoRoot }

$issues = [System.Collections.Generic.List[object]]::new()
$warnings = [System.Collections.Generic.List[object]]::new()
function Add-Issue([string]$Path, [string]$Field, [string]$Definition, [string]$Reason) {
    [void]$issues.Add([pscustomobject]@{ path = $Path; field = $Field; definition = $Definition; reason = $Reason })
}
function Add-Warning([string]$Path, [string]$Reason) {
    [void]$warnings.Add([pscustomobject]@{ path = $Path; reason = $Reason })
}
function Read-JsonFile([string]$Path) {
    try { return (Get-Content -LiteralPath $Path -Raw | ConvertFrom-Json) }
    catch { Add-Issue $Path '' '' "invalid JSON: $($_.Exception.Message)"; return $null }
}
function Relative-ToRoot([string]$Path) {
    return $Path.Substring($repoRoot.Length + 1).Replace('\','/')
}
function ContentPath-FromAsset([string]$AssetPath) {
    if ($AssetPath -notmatch '^/Game/(.+)$') { return '' }
    $relative = $Matches[1]
    if ($relative -like 'AbilityDefinitions/*') { return Join-Path 'Content' $relative }
    return $relative
}
function Assert-UniqueIds([object[]]$Items, [string]$Path, [string]$Property, [string]$Definition) {
    $seen = @{}
    foreach ($item in @($Items)) {
        $value = [string]$item.$Property
        if ([string]::IsNullOrWhiteSpace($value)) { Add-Issue $Path $Property $Definition 'required identifier is empty'; continue }
        if ($seen.ContainsKey($value)) { Add-Issue $Path $Property $value 'duplicate identifier' }
        $seen[$value] = $true
    }
}

$manifest = Read-JsonFile (Resolve-Path -LiteralPath $ManifestPath).Path
$scope = Read-JsonFile (Resolve-Path -LiteralPath $ScopePath).Path
if (-not $manifest -or -not $scope) { $manifest = $manifest; $scope = $scope }

if ($manifest) {
    if ([int]$manifest.schemaVersion -ne 1) { Add-Issue (Relative-ToRoot $ManifestPath) 'schemaVersion' 'manifest' 'must be 1' }
    if ([string]$manifest.scopeRevision -ne [string]$scope.scopeRevision) { Add-Issue (Relative-ToRoot $ManifestPath) 'scopeRevision' 'manifest' 'does not match frozen scope' }
    if ([string]$manifest.canonicalMap -ne '/Game/Maps/StartupMap') { Add-Issue (Relative-ToRoot $ManifestPath) 'canonicalMap' 'manifest' 'must be /Game/Maps/StartupMap' }
    foreach ($role in @('Aura','Crunch')) { if (@($manifest.requiredRoles) -notcontains $role) { Add-Issue (Relative-ToRoot $ManifestPath) 'requiredRoles' $role 'mandatory candidate role is missing' } }
    foreach ($topology in @('listen','dedicated')) { if (@($manifest.requiredTopologies) -notcontains $topology) { Add-Issue (Relative-ToRoot $ManifestPath) 'requiredTopologies' $topology 'mandatory candidate topology is missing' } }
    foreach ($required in @($manifest.requiredFiles)) {
        $requiredPath = Join-Path $repoRoot ([string]$required)
        if (-not (Test-Path -LiteralPath $requiredPath -PathType Leaf)) { Add-Issue ([string]$required) '' 'required-file' 'file is missing from the canonical checkout' }
        if (-not [string]::IsNullOrWhiteSpace($StagedRoot)) {
            $stagedPath = Join-Path $StagedRoot ([string]$required)
            if (-not (Test-Path -LiteralPath $stagedPath -PathType Leaf)) { Add-Issue ([string]$required) '' 'staged-required-file' 'file is missing from staged/package root' }
        }
    }
}

$configFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Content/Config') -Filter '*.json' -File | Sort-Object FullName)
$configObjects = @{}
foreach ($file in $configFiles) { $configObjects[$file.Name] = Read-JsonFile $file.FullName }

$roleConfig = $configObjects['RoleConfig.json']
$levelConfig = $configObjects['LevelConfig.json']
$itemsConfig = $configObjects['ItemDefinitions.json']
$merchantConfig = $configObjects['MerchantDefinitions.json']
$economyConfig = $configObjects['EconomyConfig.json']
$rewardConfig = $configObjects['RewardDefinitions.json']
$tutorialConfig = $configObjects['PlayableCandidateTutorial.json']

if ($roleConfig) {
    $rolePath = 'Content/Config/RoleConfig.json'
    Assert-UniqueIds @($roleConfig.roles) $rolePath 'role' 'role'
    foreach ($role in @('Aura','Crunch')) { if (@($roleConfig.roles | ForEach-Object { [string]$_.role }) -notcontains $role) { Add-Issue $rolePath 'roles' $role 'mandatory role is missing' } }
    foreach ($role in @($roleConfig.roles)) {
        $assetPath = ContentPath-FromAsset ([string]$role.lmbAbilityDefinition)
        if (-not [string]::IsNullOrWhiteSpace($assetPath) -and -not (Test-Path -LiteralPath (Join-Path $repoRoot $assetPath) -PathType Leaf)) { Add-Issue $rolePath 'lmbAbilityDefinition' ([string]$role.role) "ability definition does not resolve: $($role.lmbAbilityDefinition)" }
    }
}
if ($levelConfig) {
    $levelPath = 'Content/Config/LevelConfig.json'
    $level = @($levelConfig.levels | Where-Object { [string]$_.id -eq 'RoleBattleCivilianTest' })
    if ($level.Count -ne 1) { Add-Issue $levelPath 'levels' 'RoleBattleCivilianTest' 'canonical map alias must exist exactly once' }
    elseif ([string]$level[0].mapPath -ne '/Game/Maps/StartupMap') { Add-Issue $levelPath 'mapPath' 'RoleBattleCivilianTest' 'canonical alias does not resolve to StartupMap' }
}
if ($itemsConfig) { Assert-UniqueIds @($itemsConfig.items) 'Content/Config/ItemDefinitions.json' 'itemId' 'item' }
if ($merchantConfig) {
    $merchantPath = 'Content/Config/MerchantDefinitions.json'
    Assert-UniqueIds @($merchantConfig.offers) $merchantPath 'offerId' 'offer'
    Assert-UniqueIds @($merchantConfig.merchants) $merchantPath 'merchantDefinitionId' 'merchant'
    $itemIds = @($itemsConfig.items | ForEach-Object { [string]$_.itemId })
    foreach ($offer in @($merchantConfig.offers)) {
        if ($itemIds -notcontains [string]$offer.itemId) { Add-Issue $merchantPath 'itemId' ([string]$offer.offerId) 'offer references an unknown item' }
        if ([string]$offer.stockPolicy -eq 'finite' -and [int]$offer.initialStock -lt 0) { Add-Issue $merchantPath 'initialStock' ([string]$offer.offerId) 'finite stock cannot be negative' }
    }
    $offerIds = @($merchantConfig.offers | ForEach-Object { [string]$_.offerId })
    foreach ($merchant in @($merchantConfig.merchants)) { foreach ($offerId in @($merchant.offerIds)) { if ($offerIds -notcontains [string]$offerId) { Add-Issue $merchantPath 'offerIds' ([string]$merchant.merchantDefinitionId) 'merchant references an unknown offer' } } }
}
if ($economyConfig -and [string]$economyConfig.supportedCurrencyId -ne 'gold') { Add-Issue 'Content/Config/EconomyConfig.json' 'supportedCurrencyId' 'economy' 'candidate currency must be gold' }
if ($rewardConfig) {
    $rewardPath = 'Content/Config/RewardDefinitions.json'
    Assert-UniqueIds @($rewardConfig.rewards) $rewardPath 'rewardId' 'reward'
    $reward = @($rewardConfig.rewards | Where-Object { [string]$_.rewardId -eq 'RoleBattle.CivilianLethalReward' })
    if ($reward.Count -ne 1) { Add-Issue $rewardPath 'rewards' 'RoleBattle.CivilianLethalReward' 'required reward definition is missing or duplicated' }
    else {
        if ([string]$reward[0].outcome -ne 'CivilianLethal') { Add-Issue $rewardPath 'outcome' 'RoleBattle.CivilianLethalReward' 'outcome must be CivilianLethal' }
        if ([int]$reward[0].amount -ne 25 -or [int]$reward[0].purchasePrice -ne 25) { Add-Issue $rewardPath 'amount/purchasePrice' 'RoleBattle.CivilianLethalReward' 'reward and purchase price must both be 25' }
    }
}
if ($tutorialConfig) {
    $tutorialPath = 'Content/Config/PlayableCandidateTutorial.json'
    Assert-UniqueIds @($tutorialConfig.steps) $tutorialPath 'stepId' 'tutorial-step'
    if (@($tutorialConfig.steps).Count -ne 6) { Add-Issue $tutorialPath 'steps' 'tutorial' 'candidate tutorial must have six ordered steps' }
    $last = 0
    foreach ($step in @($tutorialConfig.steps | Sort-Object order)) { if ([int]$step.order -ne ($last + 1)) { Add-Issue $tutorialPath 'order' ([string]$step.stepId) 'tutorial order is not contiguous' }; $last = [int]$step.order; if (@($step.roles).Count -lt 2) { Add-Issue $tutorialPath 'roles' ([string]$step.stepId) 'step must be available to both player roles' } }
}

$abilityFiles = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Content/AbilityDefinitions') -Filter '*.xml' -File | Sort-Object FullName)
$abilityNames = @{}
foreach ($file in $abilityFiles) {
    $relative = Relative-ToRoot $file.FullName
    try { [xml]$xml = Get-Content -LiteralPath $file.FullName -Raw }
    catch { Add-Issue $relative '' 'ability-definition' "invalid XML: $($_.Exception.Message)"; continue }
    $ability = $xml.ability
    if (-not $ability) {
        Add-Warning $relative 'non-ability XML is outside the playable ability-definition contract'
        continue
    }
    $name = [string]$ability.name
    if ([string]::IsNullOrWhiteSpace($name)) { Add-Issue $relative 'name' 'ability-definition' 'ability name is required' } elseif ($abilityNames.ContainsKey($name)) { Add-Issue $relative 'name' $name 'duplicate ability name' } else { $abilityNames[$name] = $relative }
    if ($name -eq 'FireGun') {
        foreach ($field in @('fireMode','minimumShotInterval','magazineCapacity','reserveCapacity','shotConsumption','reloadDuration')) { if ([string]::IsNullOrWhiteSpace([string]$ability.$field)) { Add-Issue $relative $field 'FireGun' 'firearm contract field is required' } }
        if ([string]$ability.fireMode -ne 'SemiAuto') { Add-Issue $relative 'fireMode' 'FireGun' 'must be SemiAuto' }
        if ([int]$ability.shotConsumption -ne 1) { Add-Issue $relative 'shotConsumption' 'FireGun' 'must consume exactly one round' }
    }
}
if ($manifest -and $manifest.firearm) { Add-Issue (Relative-ToRoot $ManifestPath) 'firearm' 'manifest' 'retired firearm contract must not be present' }

if (-not [string]::IsNullOrWhiteSpace($MalformedFixtureRoot) -and (Test-Path -LiteralPath $MalformedFixtureRoot -PathType Container)) {
    foreach ($fixture in @(Get-ChildItem -LiteralPath $MalformedFixtureRoot -File | Where-Object { $_.Extension -in @('.json','.xml') })) {
        $valid = $true
        try { if ($fixture.Extension -eq '.json') { [void](Get-Content -LiteralPath $fixture.FullName -Raw | ConvertFrom-Json) } else { [xml](Get-Content -LiteralPath $fixture.FullName -Raw) | Out-Null } }
        catch { $valid = $false }
        if ($valid) { Add-Issue (Relative-ToRoot $fixture.FullName) '' 'malformed-fixture' 'fixture unexpectedly parsed as valid' }
    }
}

$result = [ordered]@{
    schemaVersion = 1
    manifest = (Relative-ToRoot (Resolve-Path -LiteralPath $ManifestPath).Path)
    scopeRevision = if ($manifest) { [string]$manifest.scopeRevision } else { '' }
    discovered = [ordered]@{ configs = $configFiles.Count; abilityDefinitions = $abilityFiles.Count; webUi = @(Get-ChildItem -LiteralPath (Join-Path $repoRoot 'Plugins/AuraWebUI/Content/WebUI') -Filter '*.html' -File).Count }
    issues = @($issues)
    warnings = @($warnings)
    passed = ($issues.Count -eq 0)
}
if ($AsJson) { $result | ConvertTo-Json -Depth 10; exit $(if ($issues.Count -eq 0) { 0 } else { 1 }) }
if ($issues.Count -gt 0) { $issues | ForEach-Object { Write-Error ("$($_.path) [$($_.field)] $($_.definition): $($_.reason)") }; exit 1 }
Write-Output "Playable Candidate content PASS: $($result.discovered.configs) configs, $($result.discovered.abilityDefinitions) ability definitions, $($result.discovered.webUi) WebUI pages."
exit 0
