[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Listen', 'Dedicated')]
    [string]$Mode,
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [ValidateRange(1, 65535)]
    [int]$Port = 17786,
    [ValidateRange(5, 300)]
    [int]$StartupTimeoutSeconds = 45,
    [ValidateRange(10, 300)]
    [int]$AssertionTimeoutSeconds = 70,
    [ValidateRange(15, 300)]
    [int]$RespawnTimeoutSeconds = 90,
    [ValidateRange(1, 120)]
    [int]$TeardownTimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'Aura.uproject'
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$ServerLog = Join-Path $LogDirectory "Day06-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "Day06-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "Day06-$Mode-Client2.log"
$ReportPath = Join-Path $ReportDirectory "Day06-$Mode.json"
$RunId = [Guid]::NewGuid().ToString('N')
$FixtureRoot = Join-Path $ProjectRoot "Saved\RoleBattleDay6-$Mode-$RunId"

if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path -LiteralPath $EditorExe)) { throw 'UE_ENGINE_ROOT is missing or UnrealEditor-Cmd.exe was not found.' }
if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Aura.uproject was not found at '$ProjectFile'." }

New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $FixtureRoot -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$OwnedProcesses = @()
$Assertions = [ordered]@{}
$Passed = $false
$Failure = $null
$StartedUtc = [DateTime]::UtcNow.ToString('o')

function Test-Pattern {
    param([string]$Path, [string]$Pattern)
    return (Test-Path -LiteralPath $Path) -and [bool](Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)
}

function Get-ProcessState {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return 'NotStarted' }
    try { $Process.Refresh(); if ($Process.HasExited) { return 'Exited' }; return 'Running' } catch { return 'Unavailable' }
}

function Start-OwnedProcess {
    param([string]$Name, [string[]]$Arguments)
    $Process = Start-Process -FilePath $EditorExe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    $Owned = [pscustomobject]@{ Name=$Name; Process=$Process; Arguments=@($Arguments); Status='Running'; ExitCode=$null }
    $script:OwnedProcesses += $Owned
    return $Owned
}

function Stop-OwnedProcess {
    param([pscustomobject]$Owned)
    if ($null -eq $Owned -or $null -eq $Owned.Process) { return }
    if ((Get-ProcessState $Owned.Process) -eq 'Running') { Stop-Process -Id $Owned.Process.Id -Force -ErrorAction SilentlyContinue }
    $Deadline = [DateTime]::UtcNow.AddSeconds($TeardownTimeoutSeconds)
    while ((Get-ProcessState $Owned.Process) -eq 'Running' -and [DateTime]::UtcNow -lt $Deadline) { Start-Sleep -Milliseconds 100 }
    $Owned.Status = if ((Get-ProcessState $Owned.Process) -eq 'Running') { 'TeardownTimeout' } else { 'Stopped' }
    try { $Owned.ExitCode = $Owned.Process.ExitCode } catch { $Owned.ExitCode = $null }
}

function Wait-ForPattern {
    param([string]$Path, [string]$Pattern, [int]$TimeoutSeconds, [pscustomobject]$RequiredProcess)
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (Test-Pattern $Path $Pattern) { return $true }
        if ($RequiredProcess -and (Get-ProcessState $RequiredProcess.Process) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}

try {
    $ServerUserDir = Join-Path $FixtureRoot 'Server'
    $Client1UserDir = Join-Path $FixtureRoot 'Client1'
    $Client2UserDir = Join-Path $FixtureRoot 'Client2'
    foreach ($Directory in @($ServerUserDir, $Client1UserDir, $Client2UserDir)) { New-Item -ItemType Directory -Path $Directory -Force | Out-Null }

    $ServerMap = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?listen' } else { '/Game/Maps/StartupMap' }
    $Server = Start-OwnedProcess 'Server' @(
        $ProjectFile, $ServerMap, '-server', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        "-port=$Port", '-RoleBattleDay6NetworkProbe', '-WorldPersistenceId=RoleBattleDay6', '-AuraPersistenceProvider=NULL', '-SaveToUserDir', "-UserDir=$ServerUserDir", "-abslog=$ServerLog"
    )
    if (-not (Wait-ForPattern $ServerLog 'GameNetDriver.*Listening|Browse:.*StartupMap' $StartupTimeoutSeconds $Server)) {
        throw 'Server did not reach its listening startup gate.'
    }
    $Assertions.ServerStarted = $true

    $Client1 = Start-OwnedProcess 'Client1' @(
        $ProjectFile, "127.0.0.1:${Port}?PlayerName=Day6Aura?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        '-RoleBattleDay6NetworkProbe', '-SaveToUserDir', "-UserDir=$Client1UserDir", "-abslog=$Client1Log"
    )
    $Client2 = Start-OwnedProcess 'Client2' @(
        $ProjectFile, "127.0.0.1:${Port}?PlayerName=Day6Bungee?Role=BungeeMan", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        '-RoleBattleDay6NetworkProbe', '-SaveToUserDir', "-UserDir=$Client2UserDir", "-abslog=$Client2Log"
    )

    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] Role=Aura Combat=Combat\.Magic.*ExistingSaveReconciliation=1.*LiveSwitchRejected=1' $AssertionTimeoutSeconds $Server)) {
        throw 'Aura authoritative role/profile/save/live-switch audit did not pass.'
    }
    $Assertions.AuraAuthoritativeProfiles = $true
    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] Role=BungeeMan Combat=Combat\.Gun.*ExistingSaveReconciliation=1.*LiveSwitchRejected=1' $AssertionTimeoutSeconds $Server)) {
        throw 'BungeeMan authoritative role/profile/save/live-switch audit did not pass.'
    }
    $Assertions.BungeeAuthoritativeProfiles = $true

    if (-not (Wait-ForPattern $Client1Log '\[Day6NetworkProbe\]\[Client\] Role=Aura Combat=Combat\.Magic Presentation=1 ClientRoleMutationRejected=1 AuthorityGrantMutation=0' $AssertionTimeoutSeconds $Client1)) {
        throw 'Aura client presentation or mutation-rejection audit did not pass.'
    }
    $Assertions.AuraClientPresentation = $true
    if (-not (Wait-ForPattern $Client2Log '\[Day6NetworkProbe\]\[Client\] Role=BungeeMan Combat=Combat\.Gun Presentation=1 ClientRoleMutationRejected=1 AuthorityGrantMutation=0' $AssertionTimeoutSeconds $Client2)) {
        throw 'BungeeMan client presentation or mutation-rejection audit did not pass.'
    }
    $Assertions.BungeeClientPresentation = $true

    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] PASS Role=Aura Respawns=2 Profiles=1 Idempotent=1' $RespawnTimeoutSeconds $Server)) {
        throw 'Aura did not complete two ledger-safe pawn replacements.'
    }
    $Assertions.AuraTwoRespawns = $true
    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] PASS Role=BungeeMan Respawns=2 Profiles=1 Idempotent=1' $RespawnTimeoutSeconds $Server)) {
        throw 'BungeeMan did not complete two ledger-safe pawn replacements.'
    }
    $Assertions.BungeeTwoRespawns = $true

    $Assertions.NoProbeFailure = -not (Test-Pattern $ServerLog '\[Day6NetworkProbe\]\[Server\] FAIL')
    $Assertions.NoCrashOrFatal = -not (@($ServerLog, $Client1Log, $Client2Log) | Where-Object { Test-Pattern $_ 'Fatal error:|Assertion failed:|Unhandled Exception' })
    if (-not $Assertions.NoProbeFailure) { throw 'The server reported a Day 6 probe failure.' }
    if (-not $Assertions.NoCrashOrFatal) { throw 'A Day 6 process reported a fatal/crash signature.' }
    $Passed = $true
}
catch {
    $Failure = $_.Exception.Message
}
finally {
    foreach ($Owned in @($OwnedProcesses | Sort-Object Name -Descending)) { Stop-OwnedProcess $Owned }
    $RequiredArtifacts = @($ServerLog, $Client1Log, $Client2Log)
    $MissingArtifacts = @($RequiredArtifacts | Where-Object { -not (Test-Path -LiteralPath $_) })
    if ($MissingArtifacts.Count -gt 0) {
        $Passed = $false
        $Text = "Missing artifact(s): $($MissingArtifacts -join ', ')"
        $Failure = if ($Failure) { "$Failure $Text" } else { $Text }
    }
    if (@($OwnedProcesses | Where-Object { $_.Status -eq 'TeardownTimeout' }).Count -gt 0) {
        $Passed = $false
        $Failure = if ($Failure) { "$Failure Owned process teardown timed out." } else { 'Owned process teardown timed out.' }
    }
    $FixtureRemoved = $false
    $ResolvedFixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
    $ResolvedSavedRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved'))
    if ($ResolvedFixtureRoot.StartsWith($ResolvedSavedRoot, [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $ResolvedFixtureRoot)) {
        Remove-Item -LiteralPath $ResolvedFixtureRoot -Recurse -Force
        $FixtureRemoved = -not (Test-Path -LiteralPath $ResolvedFixtureRoot)
    }
    $Report = [ordered]@{
        SchemaVersion=1; Mode=$Mode; Port=$Port; StartedUtc=$StartedUtc; CompletedUtc=[DateTime]::UtcNow.ToString('o')
        Assertions=$Assertions
        Processes=@($OwnedProcesses | ForEach-Object { [ordered]@{ Name=$_.Name; ProcessId=$_.Process.Id; Arguments=$_.Arguments; Status=$_.Status; ExitCode=$_.ExitCode } })
        Artifacts=[ordered]@{ ServerLog=$ServerLog; Client1Log=$Client1Log; Client2Log=$Client2Log; Report=$ReportPath }
        IsolatedFixtureRemoved=$FixtureRemoved; Passed=$Passed; Failure=$Failure
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

if (-not $Passed) { Write-Error "[Day6NetworkSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[Day6NetworkSmoke][$Mode] PASS"
exit 0
