[CmdletBinding()]
param(
    [string]$SourceRoot = 'C:\Works\Crunch-master',
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [string]$ProjectPath,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($ProjectPath)) {
    $ProjectPath = Join-Path $SourceRoot 'Crunch.uproject'
}
if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot 'Saved\Reports\CrunchMigration\SourceEditorPreflight.json'
}

$reportDirectory = Split-Path -Parent $ReportPath
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$started = [DateTime]::UtcNow
$failures = [System.Collections.Generic.List[string]]::new()
$observedBlockers = [System.Collections.Generic.List[string]]::new()
$checks = [ordered]@{}

try {
    $sourceExists = Test-Path -LiteralPath $SourceRoot -PathType Container
    $projectExists = Test-Path -LiteralPath $ProjectPath -PathType Leaf
    $editorPath = if ($EngineRoot) { Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe' } else { $null }
    $editorExists = $editorPath -and (Test-Path -LiteralPath $editorPath -PathType Leaf)
    $targetFiles = @('Crunch.Target.cs', 'CrunchEditor.Target.cs', 'CrunchServer.Target.cs') | ForEach-Object {
        $path = Join-Path $SourceRoot "Source\$_"
        [ordered]@{ path = "Source/$_"; exists = Test-Path -LiteralPath $path -PathType Leaf }
    }
    $checks.SourceRoot = $sourceExists
    $checks.Project = $projectExists
    $checks.Editor = [ordered]@{ path = $editorPath; exists = $editorExists }
    $checks.TargetFiles = @($targetFiles)
    if (-not $sourceExists) { $failures.Add("source checkout missing: $SourceRoot") }
    if (-not $projectExists) { $failures.Add("source project missing: $ProjectPath") }
    if (-not $editorExists) { $failures.Add("editor executable missing: $editorPath") }
    $missingTargets = @($targetFiles | Where-Object { -not $_.exists })
    if ($missingTargets.Count -gt 0) {
        $failures.Add("source target descriptors missing: $(($missingTargets.path) -join ', ')")
    }

    $logCandidates = @()
    $savedLogRoot = Join-Path $SourceRoot 'Saved\Logs'
    if (Test-Path -LiteralPath $savedLogRoot -PathType Container) {
        $logCandidates = @(Get-ChildItem -LiteralPath $savedLogRoot -File -Filter 'Crunch*.log' | Sort-Object LastWriteTime -Descending)
    }
    $latestLog = $logCandidates | Select-Object -First 1
    $checks.LatestLog = if ($latestLog) { $latestLog.FullName } else { $null }
    if ($latestLog) {
        $logText = Get-Content -LiteralPath $latestLog.FullName -Raw
        $pluginFailures = [regex]::Matches($logText, "Plugin '([^']+)' failed to load because module '([^']+)' could not be found")
        foreach ($pluginFailure in $pluginFailures) {
            $pluginName = $pluginFailure.Groups[1].Value
            $moduleName = $pluginFailure.Groups[2].Value
            $pluginBlocker = "$pluginName module ($moduleName) is missing from the selected engine; the source editor exits before commandlet execution"
            if (-not $observedBlockers.Contains($pluginBlocker)) { $observedBlockers.Add($pluginBlocker) }
        }
        if ($logText -match 'Unable to find target receipt') {
            $observedBlockers.Add('source project has no target receipt; a source-editor build is required before commandlet execution')
        }
    }
    if ($observedBlockers.Count -gt 0) { $failures.AddRange($observedBlockers) }
} catch {
    $failures.Add($_.Exception.Message)
}

$passed = $failures.Count -eq 0
$status = if ($passed) { 'READY' } elseif ($observedBlockers.Count -gt 0) { 'BLOCKED' } else { 'NOT_READY' }
$report = [ordered]@{
    schemaVersion = 1
    status = $status
    passed = $passed
    sourceRoot = $SourceRoot
    projectPath = $ProjectPath
    engineRoot = $EngineRoot
    startedUtc = $started.ToString('o')
    completedUtc = [DateTime]::UtcNow.ToString('o')
    checks = $checks
    observedBlockers = @($observedBlockers)
    failure = if ($passed) { $null } else { $failures -join '; ' }
}
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
if ($passed) {
    Write-Host '[CrunchMigration][SourceEditorPreflight] READY'
    exit 0
}
Write-Error "[CrunchMigration][SourceEditorPreflight] ${status}: $($failures -join '; ')"
exit 1
