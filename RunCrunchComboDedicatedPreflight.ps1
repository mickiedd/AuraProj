[CmdletBinding()]
param(
    [string]$ServerExe,
    [ValidateSet('Full', 'Cancel', 'BeforeClose', 'AtOrAfterClose')]
    [string]$Scenario = 'Full',
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'
$ScriptRoot = if (-not [string]::IsNullOrWhiteSpace($PSScriptRoot)) { $PSScriptRoot } else { Split-Path -Parent $MyInvocation.MyCommand.Path }
if ([string]::IsNullOrWhiteSpace($ServerExe)) { $ServerExe = Join-Path $ScriptRoot 'Binaries\Win64\AuraServer.exe' }
if ([string]::IsNullOrWhiteSpace($ReportPath)) { $ReportPath = Join-Path $ScriptRoot 'Saved\Reports\CrunchCombo-Dedicated.json' }
$ReportDirectory = Split-Path -Parent $ReportPath
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null

$EditorBinary = Join-Path $ScriptRoot 'Binaries\Win64\UnrealEditor-Aura.dll'
$Exists = Test-Path -LiteralPath $ServerExe
$FreshAgainstEditor = $Exists -and ((-not (Test-Path -LiteralPath $EditorBinary)) -or ((Get-Item -LiteralPath $ServerExe).LastWriteTimeUtc -ge (Get-Item -LiteralPath $EditorBinary).LastWriteTimeUtc))
$Ready = $Exists -and $FreshAgainstEditor
$Failure = if (-not $Exists) {
    "Dedicated server executable not found at '$ServerExe'. The installed UE distribution cannot build a Server target; provide a source-built AuraServer before running the dedicated fallback smoke."
} elseif (-not $FreshAgainstEditor) {
    "Dedicated server executable '$ServerExe' predates the current editor binary '$EditorBinary'. It is stale evidence; rebuild a matching AuraServer with a source-built/server-capable toolchain before running the dedicated fallback smoke."
} else {
    $null
}
$Report = [ordered]@{
    schemaVersion = 1
    Mode = 'Dedicated'
    Topology = 'Dedicated'
    NetMode = 'DedicatedServer'
    ServerRole = 'Authority'
    ClientRole = 'AutonomousProxy'
    scenario = "Combo$Scenario"
    networkLagMs = 0
    networkLagVarianceMs = 0
    eventSource = 'DedicatedFallback'
    status = if ($Ready) { 'READY_TO_RUN' } else { 'BLOCKED' }
    passed = $false
    failure = $Failure
    Assertions = [ordered]@{
        ServerExecutable = $Exists
        FreshAgainstEditor = $FreshAgainstEditor
        RequiresExternalClient = $true
        RequiresFallbackEventSource = $true
        NoListenSubstitution = $true
    }
    Artifacts = [ordered]@{ ServerExecutable = $ServerExe; Report = $ReportPath }
}
$Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8

if (-not $Ready) {
    Write-Error "[CrunchComboDedicatedPreflight] BLOCKED: $Failure"
    exit 2
}
Write-Host "[CrunchComboDedicatedPreflight] READY_TO_RUN Server=$ServerExe Scenario=Combo$Scenario"
exit 0
