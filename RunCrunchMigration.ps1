[CmdletBinding()]
param(
    [Parameter(Mandatory=$true)]
    [ValidateSet('Fast','Network','Package','Final')]
    [string]$Stage,
    [Parameter(Mandatory=$true)]
    [string]$Scenario,
    [Parameter(Mandatory=$true)]
    [ValidatePattern('^[A-Za-z0-9._-]+$')]
    [string]$RunId,
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [string]$PackageReportPath,
    [ValidateRange(0,1000)]
    [int]$NetworkLagMs = 0,
    [ValidateRange(0,500)]
    [int]$NetworkLagVarianceMs = 0
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SavedRoot = Join-Path $ProjectRoot 'Saved\Reports\CrunchMigration'
$ReportRoot = Join-Path $SavedRoot $RunId
$ResultPath = Join-Path $ReportRoot 'result.json'
$ScopePath = Join-Path $ProjectRoot 'Docs\Plans\Crunch-Migration\crunch-migration-scope.json'
$AssetManifestPath = Join-Path $ProjectRoot 'Docs\Plans\Crunch-Migration\crunch-source-asset-manifest.json'
$NetworkSchemaPath = Join-Path $ProjectRoot 'Docs\Plans\Crunch-Migration\crunch-network-result.schema.json'
$ExportPreflightScript = Join-Path $ProjectRoot 'RunCrunchMigrationExportPreflight.ps1'
$EditorExe = if ($EngineRoot) { Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe' } else { $null }

if (-not (Test-Path -LiteralPath $ProjectRoot)) { throw "Project root missing: $ProjectRoot" }
if (-not (Test-Path -LiteralPath $ScopePath)) { throw "Scope manifest missing: $ScopePath" }
if (-not (Test-Path -LiteralPath $AssetManifestPath)) { throw "Source asset manifest missing: $AssetManifestPath" }
if ($Stage -eq 'Network' -and -not (Test-Path -LiteralPath $NetworkSchemaPath)) { throw "Network result schema missing: $NetworkSchemaPath" }
New-Item -ItemType Directory -Path $ReportRoot -Force | Out-Null

$started = [DateTime]::UtcNow
$failures = [System.Collections.Generic.List[string]]::new()
$assertions = [ordered]@{
    Stage = $Stage
    Scenario = $Scenario
    RunId = $RunId
    NetworkLagMs = $NetworkLagMs
    NetworkLagVarianceMs = $NetworkLagVarianceMs
    ScopeManifest = $ScopePath
    SourceAssetManifest = $AssetManifestPath
	NetworkResultSchema = $NetworkSchemaPath
}
$exportPreflightScenarios = @('Assets','Uppercut','Dash','AreaSkills','GroundBlast','Tornado','Role','Package','Final')
$reportArtifactsExportPreflight = $null

try {
    $scope = Get-Content -LiteralPath $ScopePath -Raw | ConvertFrom-Json
    $assets = Get-Content -LiteralPath $AssetManifestPath -Raw | ConvertFrom-Json
    if ($scope.inputMap.'InputTag.LMB' -ne 'Abilities.Melee.CrunchCombo') { $failures.Add('LMB scope mapping is not CrunchCombo') }
    if ($scope.inputMap.'InputTag.1' -ne 'Abilities.Melee.CrunchUppercut') { $failures.Add('InputTag.1 scope mapping is not CrunchUppercut') }
    if ($scope.inputMap.'InputTag.2' -ne 'Abilities.Melee.CrunchDash') { $failures.Add('InputTag.2 scope mapping is not CrunchDash') }
    if ($scope.inputMap.'InputTag.3' -ne 'Abilities.Melee.CrunchGroundBlast') { $failures.Add('InputTag.3 scope mapping is not CrunchGroundBlast') }
    if ($scope.inputMap.'InputTag.4' -ne 'Abilities.Melee.CrunchTornado') { $failures.Add('InputTag.4 scope mapping is not CrunchTornado') }
    if ($scope.authority.fallbackTimeline -ne 'dedicated-server-only') { $failures.Add('fallback timeline is not dedicated-server-only') }
    if ($Stage -in @('Network','Package','Final') -and -not $EditorExe) { $failures.Add('EngineRoot is required for this stage') }
    if ($EditorExe -and $Stage -in @('Network','Package','Final') -and -not (Test-Path -LiteralPath $EditorExe)) { $failures.Add("UnrealEditor-Cmd.exe not found: $EditorExe") }
    if ($Stage -eq 'Final' -and [string]::IsNullOrWhiteSpace($PackageReportPath)) { $failures.Add('Final stage requires an explicit PackageReportPath; latest selection is forbidden') }
	if ($Stage -eq 'Network' -and $Scenario -notin @('ComboFull','ComboCancel','ComboBeforeClose','ComboAtOrAfterClose')) { $failures.Add('Network stage requires a supported Crunch combo scenario') }
    if ($assets.requiredAnchors.Count -lt 18) { $failures.Add('source asset anchor inventory has fewer than 18 rows') }
    if ($assets.requiredEditorExport.Count -lt 6) { $failures.Add('source asset manifest is missing required editor export lanes') }

    # Days 04–10 consume frozen editor exports. Keep this gate explicit: a
    # missing export must block the requested scenario rather than silently
    # allowing guessed defaults or prototype assets to advance.
    if ($Scenario -in $exportPreflightScenarios) {
        if (-not (Test-Path -LiteralPath $ExportPreflightScript)) {
            $failures.Add("source editor export preflight missing: $ExportPreflightScript")
        } else {
            $exportPreflightReport = Join-Path $ReportRoot 'source-export-preflight.json'
            $runnerErrorAction = $ErrorActionPreference
            try {
                # The child intentionally returns nonzero for BLOCKED. Capture
                # that result as data instead of letting its error stream abort
                # this runner before the parent report is written.
                $ErrorActionPreference = 'Continue'
                $preflightOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $ExportPreflightScript `
                    -SourceRoot ([string]$assets.sourceRoot) `
                    -ManifestPath $AssetManifestPath `
                    -ReportPath $exportPreflightReport 2>&1 | ForEach-Object { $_.ToString() }
            } finally {
                $ErrorActionPreference = $runnerErrorAction
            }
            $preflightExitCode = $LASTEXITCODE
            $assertions.ExportPreflight = [ordered]@{
                Script = $ExportPreflightScript
                Report = $exportPreflightReport
                ExitCode = $preflightExitCode
                Output = ($preflightOutput | Out-String).Trim()
            }
            $reportArtifactsExportPreflight = $exportPreflightReport
            if ($preflightExitCode -ne 0) {
                $failures.Add("source editor export preflight blocked scenario $Scenario; see $exportPreflightReport")
            }
        }
    }
    $assertions.ScopeContract = ($failures.Count -eq 0)
} catch {
    $failures.Add($_.Exception.Message)
}

$passed = $failures.Count -eq 0
$artifacts = [ordered]@{ Result = $ResultPath; Scope = $ScopePath; SourceAssets = $AssetManifestPath }
if ($reportArtifactsExportPreflight) { $artifacts.ExportPreflight = $reportArtifactsExportPreflight }
$report = [ordered]@{
    SchemaVersion = 1
    Stage = $Stage
    Scenario = $Scenario
    RunId = $RunId
    StartedUtc = $started.ToString('o')
    CompletedUtc = [DateTime]::UtcNow.ToString('o')
    Assertions = $assertions
    Artifacts = $artifacts
    Passed = $passed
    Failure = if ($passed) { $null } else { $failures -join '; ' }
}
$report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ResultPath -Encoding UTF8
if (-not $passed) { Write-Error "[CrunchMigration][$Stage][$Scenario] FAIL: $($failures -join '; ')"; exit 1 }
Write-Host "[CrunchMigration][$Stage][$Scenario] PASS (contract preflight)"
exit 0
