[CmdletBinding()]
param(
    [string]$SourceRoot = 'C:\Works\Crunch-master',
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [string]$ManifestPath,
    [string]$ExportRoot,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$SourceEditorPreflightScript = Join-Path $ProjectRoot 'RunCrunchSourceEditorPreflight.ps1'
if ([string]::IsNullOrWhiteSpace($ManifestPath)) {
    $ManifestPath = Join-Path $ProjectRoot 'Docs\Plans\Crunch-Migration\crunch-source-asset-manifest.json'
}
if ([string]::IsNullOrWhiteSpace($ExportRoot)) {
    $ExportRoot = Join-Path $SourceRoot 'Saved\CrunchMigration\Exports'
}
if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path $ProjectRoot 'Saved\Reports\CrunchMigration\ExportPreflight.json'
}

$reportDirectory = Split-Path -Parent $ReportPath
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$started = [DateTime]::UtcNow
$failures = [System.Collections.Generic.List[string]]::new()
$anchorFailures = [System.Collections.Generic.List[string]]::new()
$missingExports = [System.Collections.Generic.List[string]]::new()
$anchorResults = [System.Collections.Generic.List[object]]::new()
$exportResults = [System.Collections.Generic.List[object]]::new()
$sourceEditorPreflight = $null

# The editor export commandlet may use either the stable lane name or a
# kebab-case filename. Both spellings are accepted, but no arbitrary file is.
$exportFileNames = [ordered]@{
    dependencyClosure = @('dependencyClosure.json', 'dependency-closure.json')
    blueprintDefaults = @('blueprintDefaults.json', 'blueprint-defaults.json')
    montageSectionTimes = @('montageSectionTimes.json', 'montage-section-times.json')
    montageNotifyTimes = @('montageNotifyTimes.json', 'montage-notify-times.json')
    gameplayCueReferences = @('gameplayCueReferences.json', 'gameplay-cue-references.json')
    sourceClassReferenceScan = @('sourceClassReferenceScan.json', 'source-class-reference-scan.json')
}

try {
    if (-not (Test-Path -LiteralPath $ManifestPath)) {
        throw "Source asset manifest missing: $ManifestPath"
    }
    if (-not (Test-Path -LiteralPath $SourceRoot)) {
        throw "Source checkout missing: $SourceRoot"
    }

    $manifest = Get-Content -LiteralPath $ManifestPath -Raw | ConvertFrom-Json
    if ($manifest.schemaVersion -ne 1) {
        $failures.Add("Unsupported source asset manifest schema: $($manifest.schemaVersion)")
    }

    if (-not (Test-Path -LiteralPath $SourceEditorPreflightScript -PathType Leaf)) {
        $failures.Add("source editor preflight missing: $SourceEditorPreflightScript")
    } else {
        $sourceEditorReportPath = Join-Path $reportDirectory 'source-editor-preflight.json'
        $preflightErrorAction = $ErrorActionPreference
        try {
            $ErrorActionPreference = 'Continue'
            $sourceEditorOutput = & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $SourceEditorPreflightScript `
                -SourceRoot $SourceRoot `
                -EngineRoot $EngineRoot `
                -ProjectPath (Join-Path $SourceRoot 'Crunch.uproject') `
                -ReportPath $sourceEditorReportPath 2>&1 | ForEach-Object { $_.ToString() }
        } finally {
            $ErrorActionPreference = $preflightErrorAction
        }
        $sourceEditorExitCode = $LASTEXITCODE
        $sourceEditorPreflight = [ordered]@{
            script = $SourceEditorPreflightScript
            report = $sourceEditorReportPath
            exitCode = $sourceEditorExitCode
            output = ($sourceEditorOutput | Out-String).Trim()
        }
        if ($sourceEditorExitCode -ne 0) {
            $failures.Add("source editor preflight blocked export generation; see $sourceEditorReportPath")
        }
    }

    foreach ($anchor in @($manifest.requiredAnchors)) {
        $anchorPath = Join-Path $SourceRoot ($anchor.path -replace '/', '\')
        $exists = Test-Path -LiteralPath $anchorPath -PathType Leaf
        $actualHash = $null
        $hashMatches = $false
        if ($exists) {
            $actualHash = (Get-FileHash -Algorithm SHA256 -LiteralPath $anchorPath).Hash
            $hashMatches = $actualHash -eq $anchor.sha256
        }
        $anchorResults.Add([ordered]@{
            path = $anchor.path
            exists = $exists
            expectedSha256 = $anchor.sha256
            actualSha256 = $actualHash
            hashMatches = $hashMatches
        })
        if (-not $exists) {
            $anchorFailures.Add("missing source anchor: $($anchor.path)")
        } elseif (-not $hashMatches) {
            $anchorFailures.Add("source anchor hash mismatch: $($anchor.path)")
        }
    }

    if (-not (Test-Path -LiteralPath $ExportRoot -PathType Container)) {
        foreach ($lane in @($manifest.requiredEditorExport)) {
            $missingExports.Add([string]$lane)
            $exportResults.Add([ordered]@{
                lane = [string]$lane
                expectedFileNames = @($exportFileNames[[string]$lane])
                present = $false
                path = $null
                sizeBytes = $null
                sha256 = $null
            })
        }
    } else {
        $filesByName = @{}
        foreach ($file in Get-ChildItem -LiteralPath $ExportRoot -Recurse -File) {
            $filesByName[$file.Name.ToLowerInvariant()] = $file
        }
        foreach ($lane in @($manifest.requiredEditorExport)) {
            $laneName = [string]$lane
            $acceptedNames = @($exportFileNames[$laneName])
            if ($acceptedNames.Count -eq 0) {
                $acceptedNames = @("$laneName.json")
            }
            $candidate = $null
            foreach ($name in $acceptedNames) {
                if ($filesByName.ContainsKey($name.ToLowerInvariant())) {
                    $candidate = $filesByName[$name.ToLowerInvariant()]
                    break
                }
            }
            $present = $null -ne $candidate
            $hash = if ($present) { (Get-FileHash -Algorithm SHA256 -LiteralPath $candidate.FullName).Hash } else { $null }
            $exportResults.Add([ordered]@{
                lane = $laneName
                expectedFileNames = $acceptedNames
                present = $present
                path = if ($present) { $candidate.FullName } else { $null }
                sizeBytes = if ($present) { $candidate.Length } else { $null }
                sha256 = $hash
            })
            if (-not $present) { $missingExports.Add($laneName) }
        }
    }

    if ($anchorFailures.Count -gt 0) { $failures.AddRange($anchorFailures) }
    if ($missingExports.Count -gt 0) {
        $failures.Add("required editor exports missing: $($missingExports -join ', ')")
    }
} catch {
    $failures.Add($_.Exception.Message)
}

$passed = $failures.Count -eq 0
$report = [ordered]@{
    schemaVersion = 1
    status = if ($passed) { 'READY' } else { 'BLOCKED' }
    passed = $passed
    sourceRoot = $SourceRoot
    manifestPath = $ManifestPath
    exportRoot = $ExportRoot
    startedUtc = $started.ToString('o')
    completedUtc = [DateTime]::UtcNow.ToString('o')
    requiredEditorExport = @($manifest.requiredEditorExport)
    anchors = @($anchorResults)
    exports = @($exportResults)
    missingExports = @($missingExports)
    anchorFailures = @($anchorFailures)
    sourceEditorPreflight = $sourceEditorPreflight
    failure = if ($passed) { $null } else { $failures -join '; ' }
}
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
if ($passed) {
    Write-Host "[CrunchMigration][ExportPreflight] READY: all anchors and editor exports are present"
    exit 0
}
Write-Error "[CrunchMigration][ExportPreflight] BLOCKED: $($failures -join '; ')"
exit 1
