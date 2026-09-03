[CmdletBinding()]
param(
    [string]$SourceRoot = 'C:\Works\Crunch-master',
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [string]$ProjectPath,
    [string]$ExportScript = 'Scripts\Tests\crunch_source_editor_export.py',
    [string]$LogPath,
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
if ([string]::IsNullOrWhiteSpace($ProjectPath)) { $ProjectPath = Join-Path $SourceRoot 'Crunch.uproject' }
if ([string]::IsNullOrWhiteSpace($EngineRoot)) { throw 'EngineRoot is required (set UE_ENGINE_ROOT or pass -EngineRoot).' }
if ([string]::IsNullOrWhiteSpace($LogPath)) { $LogPath = Join-Path $SourceRoot 'Saved\Logs\Crunch-source-editor-export.log' }
if ([string]::IsNullOrWhiteSpace($ReportPath)) { $ReportPath = Join-Path $ProjectRoot 'Saved\Reports\CrunchMigration\SourceEditorExport.json' }
$editor = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$scriptPath = if ([IO.Path]::IsPathRooted($ExportScript)) { $ExportScript } else { Join-Path $ProjectRoot $ExportScript }
$reportDirectory = Split-Path -Parent $ReportPath
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
New-Item -ItemType Directory -Path (Split-Path -Parent $LogPath) -Force | Out-Null

$started = [DateTime]::UtcNow
$checks = [ordered]@{
    sourceRoot = [ordered]@{ path = $SourceRoot; exists = Test-Path -LiteralPath $SourceRoot -PathType Container }
    project = [ordered]@{ path = $ProjectPath; exists = Test-Path -LiteralPath $ProjectPath -PathType Leaf }
    editor = [ordered]@{ path = $editor; exists = Test-Path -LiteralPath $editor -PathType Leaf }
    exportScript = [ordered]@{ path = $scriptPath; exists = Test-Path -LiteralPath $scriptPath -PathType Leaf }
}
$failure = $null
$exitCode = $null
try {
    if (-not $checks.sourceRoot.exists) { throw "source checkout missing: $SourceRoot" }
    if (-not $checks.project.exists) { throw "source project missing: $ProjectPath" }
    if (-not $checks.editor.exists) { throw "editor executable missing: $editor" }
    if (-not $checks.exportScript.exists) { throw "export script missing: $scriptPath" }
    $scriptArg = "-Script=exec(open('$($scriptPath.Replace('\', '/'))').read())"
    $editorArgs = @(
        $ProjectPath,
        '-run=PythonScript',
        $scriptArg,
        '-unattended', '-nop4', '-nullrhi', '-stdout', '-FullStdOutLogOutput', '-nosplash',
        "-abslog=$LogPath"
    )
    & $editor @editorArgs *> $null
    $exitCode = $LASTEXITCODE
    if ($exitCode -ne 0) { throw "source editor export commandlet failed with exit code $exitCode; see $LogPath" }
} catch {
    $failure = $_.Exception.Message
}
$passed = $null -eq $failure -and $exitCode -eq 0
$report = [ordered]@{
    schemaVersion = 1
    status = if ($passed) { 'READY' } else { 'BLOCKED' }
    passed = $passed
    sourceRoot = $SourceRoot
    projectPath = $ProjectPath
    engineRoot = $EngineRoot
    exportScript = $scriptPath
    logPath = $LogPath
    startedUtc = $started.ToString('o')
    completedUtc = [DateTime]::UtcNow.ToString('o')
    checks = $checks
    editorExitCode = $exitCode
    failure = $failure
}
$report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
if ($passed) { Write-Host '[CrunchMigration][SourceEditorExport] READY'; exit 0 }
Write-Error "[CrunchMigration][SourceEditorExport] BLOCKED: $failure"
exit 1
