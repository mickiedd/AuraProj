[CmdletBinding()]
param(
    [ValidateSet('Listen', 'Dedicated')]
    [string]$Mode,
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [ValidateRange(1, 65535)]
    [int]$ListenPort = 17779,
    [ValidateRange(1, 300)]
    [int]$StartupTimeoutSeconds = 30,
    [ValidateRange(1, 300)]
    [int]$AssertionTimeoutSeconds = 45,
    [ValidateRange(1, 120)]
    [int]$TeardownTimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'Aura.uproject'
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$ServerCandidates = @(
    (Join-Path $ProjectRoot 'Binaries\Win64\AuraServer.exe'),
    (Join-Path $ProjectRoot 'Binaries\Win64\AuraServer-Win64-Development.exe'),
    (Join-Path $ProjectRoot 'Binaries\Win64\AuraServer-Win64-DebugGame.exe')
)
$CookedStartupMap = Join-Path $ProjectRoot 'Saved\Cooked\WindowsServer\Aura\Content\Maps\StartupMap.umap'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'

if ([string]::IsNullOrWhiteSpace($Mode)) {
    Write-Error 'Specify -Mode Listen or -Mode Dedicated explicitly.'
    exit 1
}
if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path -LiteralPath $EditorExe)) {
    Write-Error 'UE_ENGINE_ROOT is missing or UnrealEditor-Cmd.exe was not found.'
    exit 1
}
if (-not (Test-Path -LiteralPath $ProjectFile)) {
    Write-Error "Aura.uproject was not found at '$ProjectFile'."
    exit 1
}

New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null

$ServerExe = $EditorExe
$ServerRuntime = 'EditorServer'
if ($Mode -eq 'Dedicated') {
    if (Test-Path -LiteralPath $CookedStartupMap) {
        $ServerExe = $ServerCandidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
        if ([string]::IsNullOrWhiteSpace($ServerExe)) {
            Write-Error 'The cooked StartupMap exists but no AuraServer executable was found. Run BuildDedicatedServer.bat.'
            exit 1
        }
        $ServerRuntime = 'PackagedDedicated'
    }
    else {
        Write-Warning 'Cooked StartupMap is unavailable; using UnrealEditor-Cmd.exe -server for the Dedicated smoke.'
    }
}

function Test-Pattern {
    param([string]$Path, [string]$Pattern)
    if (-not (Test-Path -LiteralPath $Path)) { return $false }
    return [bool](Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)
}

function Get-ProcessState {
    param([System.Diagnostics.Process]$Process)
    if ($null -eq $Process) { return 'NotStarted' }
    try {
        $Process.Refresh()
        if ($Process.HasExited) { return 'Exited' }
        return 'Running'
    }
    catch { return 'Unavailable' }
}

function Start-OwnedProcess {
    param([string]$Name, [string]$Executable, [string[]]$Arguments)
    $Process = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    return [pscustomobject]@{
        Name = $Name
        Process = $Process
        Executable = $Executable
        Arguments = @($Arguments)
        Status = 'Running'
        ExitCode = $null
    }
}

function Stop-OwnedProcess {
    param([pscustomobject]$Owned, [int]$TimeoutSeconds)
    if ($null -eq $Owned -or $null -eq $Owned.Process) { return }
    $Process = $Owned.Process
    if ((Get-ProcessState $Process) -eq 'Running') {
        Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
    }
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    while ((Get-ProcessState $Process) -eq 'Running' -and [DateTime]::UtcNow -lt $Deadline) {
        Start-Sleep -Milliseconds 100
    }
    $Owned.Status = if ((Get-ProcessState $Process) -eq 'Running') { 'TeardownTimeout' } else { 'Stopped' }
    try { $Owned.ExitCode = $Process.ExitCode } catch { $Owned.ExitCode = $null }
}

$Port = $ListenPort
$ServerLog = Join-Path $LogDirectory "Day03-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "Day03-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "Day03-$Mode-Client2.log"
$ReportPath = Join-Path $ReportDirectory "Day03-$Mode.json"
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$OwnedProcesses = @()
$Passed = $false
$Failure = $null
$Assertions = [ordered]@{}
$StartedUtc = [DateTime]::UtcNow.ToString('o')

try {
    $ServerMap = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?listen' } else { '/Game/Maps/StartupMap' }
    $ServerArguments = @(
        $(if ($ServerRuntime -eq 'PackagedDedicated') { $ServerMap } else { $ProjectFile }),
        $(if ($ServerRuntime -eq 'PackagedDedicated') { '' } else { $ServerMap }),
        '-server', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        "-port=$Port", '-RoleBattleDay3NetworkProbe', "-abslog=$ServerLog"
    ) | Where-Object { -not [string]::IsNullOrWhiteSpace($_) }
    $Server = Start-OwnedProcess 'Server' $ServerExe $ServerArguments
    $OwnedProcesses += $Server

    $StartupDeadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
    while (-not (Test-Path -LiteralPath $ServerLog) -and [DateTime]::UtcNow -lt $StartupDeadline) {
        if ((Get-ProcessState $Server.Process) -ne 'Running') {
            throw "Server exited before creating '$ServerLog'."
        }
        Start-Sleep -Milliseconds 250
    }
    if (-not (Test-Path -LiteralPath $ServerLog)) { throw "Server startup timed out; '$ServerLog' is missing." }

    $ClientBaseArguments = @(
        $ProjectFile, "127.0.0.1:$Port", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-RoleBattleDay3NetworkProbe'
    )
    foreach ($ClientSpec in @(@('Client1', $Client1Log, 'Aura'), @('Client2', $Client2Log, 'BungeeMan'))) {
        $ClientArguments = @($ClientBaseArguments)
        $ClientArguments[1] = "127.0.0.1:${Port}?PlayerName=Day3$($ClientSpec[2])?Role=$($ClientSpec[2])"
        $ClientArguments += "-abslog=$($ClientSpec[1])"
        $Client = Start-OwnedProcess $ClientSpec[0] $EditorExe $ClientArguments
        $OwnedProcesses += $Client
        Start-Sleep -Milliseconds 500
    }

    $AssertionDeadline = [DateTime]::UtcNow.AddSeconds($AssertionTimeoutSeconds)
    do {
        foreach ($Owned in $OwnedProcesses) {
            if ((Get-ProcessState $Owned.Process) -ne 'Running') {
                throw "$($Owned.Name) exited before network assertions completed."
            }
        }
        $Assertions = [ordered]@{
            ServerStateTransitionDying = Test-Pattern $ServerLog '\[Day3NetworkProbe\]\[Server\] StateTransitionDying=1'
            ServerStateTransitionDead = Test-Pattern $ServerLog '\[Day3NetworkProbe\]\[Server\] StateTransitionDead=1'
            ServerCivilianDefaultDenied = Test-Pattern $ServerLog '\[Day3NetworkProbe\]\[Server\] CivilianDefaultPolicyDenied=1'
            Client1StateMutationDenied = Test-Pattern $Client1Log '\[Day3NetworkProbe\]\[Client\] ClientStateMutationAccepted=0'
            Client1PolicyMutationDenied = Test-Pattern $Client1Log '\[Day3NetworkProbe\]\[Client\].*ClientStateMutationAccepted=0.*ClientPolicyMutationAccepted=0'
            Client1StateReplicated = Test-Pattern $Client1Log '\[Day3NetworkProbe\]\[Client\] ReplicatedState=(Dying|Dead)'
            Client2StateMutationDenied = Test-Pattern $Client2Log '\[Day3NetworkProbe\]\[Client\].*ClientStateMutationAccepted=0.*ClientPolicyMutationAccepted=0'
            EnemyStillTargetsPlayer = Test-Pattern $ServerLog '\[EnemyAI\]\[FindNearestPlayer\].*Closest=.*BP_AuraCharacter'
        }
        if (@($Assertions.Values | Where-Object { -not [bool]$_ }).Count -eq 0) { break }
        Start-Sleep -Seconds 1
    } while ([DateTime]::UtcNow -lt $AssertionDeadline)

    $Failed = @($Assertions.GetEnumerator() | Where-Object { -not [bool]$_.Value } | ForEach-Object { $_.Key })
    if ($Failed.Count -gt 0) { throw "Network assertions timed out: $($Failed -join ', ')" }
    $Passed = $true
}
catch {
    $Failure = $_.Exception.Message
}
finally {
    foreach ($Owned in @($OwnedProcesses | Sort-Object Name -Descending)) {
        Stop-OwnedProcess $Owned $TeardownTimeoutSeconds
    }
    $MissingArtifacts = @($ServerLog, $Client1Log, $Client2Log) | Where-Object { -not (Test-Path -LiteralPath $_) }
    if ($MissingArtifacts.Count -gt 0) {
        $Passed = $false
        $MissingMessage = "Missing artifact(s): $($MissingArtifacts -join ', ')"
        $Failure = if ([string]::IsNullOrWhiteSpace($Failure)) { $MissingMessage } else { "$Failure $MissingMessage" }
    }
    if (@($OwnedProcesses | Where-Object { $_.Status -eq 'TeardownTimeout' }).Count -gt 0) {
        $Passed = $false
        $Failure = if ([string]::IsNullOrWhiteSpace($Failure)) { 'Owned process teardown timed out.' } else { "$Failure Owned process teardown timed out." }
    }
    $Report = [ordered]@{
        SchemaVersion = 1
        Mode = $Mode
        ServerRuntime = $ServerRuntime
        Port = $Port
        StartedUtc = $StartedUtc
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
        Assertions = $Assertions
        Processes = @($OwnedProcesses | ForEach-Object {
            [ordered]@{ Name=$_.Name; ProcessId=$_.Process.Id; Executable=$_.Executable; Arguments=$_.Arguments; Status=$_.Status; ExitCode=$_.ExitCode }
        })
        Artifacts = [ordered]@{ ServerLog=$ServerLog; Client1Log=$Client1Log; Client2Log=$Client2Log; Report=$ReportPath }
        Passed = $Passed
        Failure = $Failure
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

if (-not $Passed) {
    Write-Error "[Day3NetworkSmoke][$Mode] FAIL: $Failure"
    exit 1
}
Write-Host "[Day3NetworkSmoke][$Mode] PASS"
exit 0
