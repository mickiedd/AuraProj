[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [ValidateSet('Listen', 'Dedicated')] [string]$Mode,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [ValidateRange(1, 65535)] [int]$Port = 19919,
    [ValidateRange(30, 300)] [int]$TimeoutSeconds = 180,
    [ValidateRange(0, 1000)] [int]$NetworkLagMs = 100,
    [ValidateRange(0, 500)] [int]$NetworkJitterMs = 20,
    [ValidateRange(0, 100)] [int]$PacketLossPercent = 2,
    [ValidateRange(1, 300)] [int]$WarmupSeconds = 30,
    [ValidateRange(5, 600)] [int]$SampleSeconds = 60
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$DedicatedExe = Join-Path $ProjectRoot 'Binaries\Win64\AuraServer.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$DayLabel = 'Day19'
$WorldId = 'Day19WorldFixture' + ([guid]::NewGuid().ToString('N'))
$ServerLog = if ($Mode -eq 'Dedicated') {
    # Packaged server processes resolve -abslog inside their cooked sandbox.
    Join-Path $ProjectRoot "Saved\Cooked\WindowsServer\Aura\Saved\Logs\$DayLabel-$Mode-Server.log"
} else {
    Join-Path $LogDirectory "$DayLabel-$Mode-Server.log"
}
$Client1Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client2.log"
$ReportPath = Join-Path $ReportDirectory "$DayLabel-$Mode.json"
if (-not (Test-Path -LiteralPath $EditorExe)) { throw "UnrealEditor-Cmd.exe was not found at '$EditorExe'." }
if ($Mode -eq 'Dedicated' -and -not (Test-Path -LiteralPath $DedicatedExe)) { throw "AuraServer.exe was not found at '$DedicatedExe'; build the required dedicated target first." }
$CookedStartupMap = Join-Path $ProjectRoot 'Saved\Cooked\WindowsServer\Aura\Content\Maps\StartupMap.umap'
if ($Mode -eq 'Dedicated' -and -not (Test-Path -LiteralPath $CookedStartupMap)) {
    throw "Dedicated cooked StartupMap was not found at '$CookedStartupMap'; cook the WindowsServer target before running the dedicated matrix."
}
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$Assertions = [ordered]@{}
$Owned = @()
$PerformanceSamples = @()
$Passed = $false
$Failure = $null
$StartedAt = [DateTime]::UtcNow

function Get-State($Item) {
    try { $Item.Process.Refresh(); if ($Item.Process.HasExited) { return 'Exited' }; return 'Running' }
    catch { return 'Unavailable' }
}

function Start-Owned([string]$Name, [string]$Executable, [string[]]$ArgumentList, [string]$LogPath) {
    $Process = Start-Process -FilePath $Executable -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden
    $Item = [pscustomobject]@{ Name = $Name; Process = $Process; Arguments = $ArgumentList; LogPath = $LogPath }
    $script:Owned += $Item
    return $Item
}

function Stop-Owned($Item) {
    if ($Item -and (Get-State $Item) -eq 'Running') { Stop-Process -Id $Item.Process.Id -Force -ErrorAction SilentlyContinue }
}

function Wait-Pattern([string]$Path, [string]$Pattern, $Item) {
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if ((Test-Path -LiteralPath $Path) -and (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) { return $true }
        if ($Item -and (Get-State $Item) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}

function Wait-Count([string]$Path, [string]$Pattern, [int]$Minimum, $Item) {
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (Test-Path -LiteralPath $Path) {
            $Matches = @(Select-String -LiteralPath $Path -Pattern $Pattern)
            if ($Matches.Count -ge $Minimum) { return $true }
        }
        if ($Item -and (Get-State $Item) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}

function Get-Percentile([object[]]$Values, [double]$Percentile) {
    $Sorted = @($Values | ForEach-Object { [double]$_ } | Sort-Object)
    if ($Sorted.Count -eq 0) { return 0.0 }
    $Index = [Math]::Min($Sorted.Count - 1, [Math]::Max(0, [int][Math]::Ceiling($Sorted.Count * $Percentile) - 1))
    return [double]$Sorted[$Index]
}

function Get-PerformanceSample($Item, [DateTime]$PreviousTime, [double]$PreviousCpuSeconds) {
    $Item.Process.Refresh()
    $Now = [DateTime]::UtcNow
    $CpuSeconds = $Item.Process.TotalProcessorTime.TotalSeconds
    $Elapsed = [Math]::Max(0.001, ($Now - $PreviousTime).TotalSeconds)
    $CpuPercent = (($CpuSeconds - $PreviousCpuSeconds) / ($Elapsed * [Environment]::ProcessorCount)) * 100.0
    return [pscustomobject]@{
        TimeUtc = $Now.ToString('o')
        CpuPercent = [Math]::Max(0.0, $CpuPercent)
        WorkingSetMiB = $Item.Process.WorkingSet64 / 1MB
        NextTime = $Now
        NextCpuSeconds = $CpuSeconds
    }
}

try {
    & py (Join-Path $ProjectRoot 'Scripts\test_role_battle_days_19.py')
    if ($LASTEXITCODE -ne 0) { throw 'Focused Day 19 contracts failed.' }
    $Assertions.StaticContracts = $true

    $Day18Runner = Join-Path $ProjectRoot 'RunRoleBattleDay18PersistenceSmoke.ps1'
    $PowerShellExe = (Get-Command powershell.exe).Source
    $PrerequisiteArgs = @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', $Day18Runner,
        '-Mode', $Mode, '-Port', ($Port + 100), '-TimeoutSeconds', $TimeoutSeconds)
    & $PowerShellExe @PrerequisiteArgs
    if ($LASTEXITCODE -ne 0) { throw 'Day 18 reconnect/isolation prerequisite did not pass.' }
    $Assertions.Day18ReconnectPrerequisite = $true

    $ServerExecutable = if ($Mode -eq 'Dedicated') { $DedicatedExe } else { $EditorExe }
    $Map = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?Role=Aura?listen' } else { '/Game/Maps/StartupMap' }
    $ServerArguments = if ($Mode -eq 'Dedicated') {
        @($Map, '-server', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash')
    } else {
        @((Join-Path $ProjectRoot 'Aura.uproject'), $Map, '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash')
    }
    $ServerArguments += @("-port=$Port", "-WorldPersistenceId=$WorldId", '-AuraPersistenceProvider=NULL', '-RoleBattleDay19MultiplayerProbe', '-RoleBattleDay19PerformanceProbe',
        "-PktLag=$NetworkLagMs", "-PktLagVariance=$NetworkJitterMs", "-PktLoss=$PacketLossPercent", "-abslog=$ServerLog")
    $Server = Start-Owned 'Server' $ServerExecutable $ServerArguments $ServerLog
    if (-not (Wait-Pattern $ServerLog '\[WorldReadiness\] State=Ready' $Server)) { throw 'Day 19 server world readiness did not complete.' }
    $Assertions.ServerWorldReady = $true

    $ClientCommon = @('-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        "-PktLag=$NetworkLagMs", "-PktLagVariance=$NetworkJitterMs", "-PktLoss=$PacketLossPercent")
    $Client1Arguments = @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=SameDisplay?Role=Aura?AuraFixtureIdentity=Day19ProfileA") + $ClientCommon + @('-RoleBattleDay19MultiplayerProbeClientA', "-abslog=$Client1Log")
    $Client2Arguments = @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=SameDisplay?Role=Aura?AuraFixtureIdentity=Day19ProfileB") + $ClientCommon + @('-RoleBattleDay19MultiplayerProbeClientB', "-abslog=$Client2Log")
    $Client1 = Start-Owned 'Client1' $EditorExe $Client1Arguments $Client1Log
    Start-Sleep -Seconds 2
    $Client2 = Start-Owned 'Client2' $EditorExe $Client2Arguments $Client2Log

    if (-not (Wait-Pattern $ServerLog '\[Day19NetworkProbe\]\[Server\] Matrix=PASS' $Server)) { throw 'Day 19 security matrix did not reach the server barrier.' }
    $Assertions.SecurityMatrix = $true
    if (-not (Wait-Pattern $Client1Log '\[Day19NetworkProbe\]\[Client\] ProbeSequenceComplete=1' $Client1)) { throw 'Client 1 did not complete the security/replay sequence.' }
    if (-not (Wait-Pattern $Client2Log '\[Day19NetworkProbe\]\[Client\] ProbeSequenceComplete=1' $Client2)) { throw 'Client 2 did not complete the security/replay sequence.' }
    if (-not (Wait-Count $Client1Log '\[Day19NetworkProbe\]\[Client\] LateJoinObserved=1' 1 $Client1) -or -not (Wait-Count $Client2Log '\[Day19NetworkProbe\]\[Client\] LateJoinObserved=1' 1 $Client2)) { throw 'A remote client did not observe replicated late-join state.' }
    $Assertions.LateJoinReplication = $true

    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*request=1 result=0 replay=0 ' 1 $Server)) { throw 'No successful authoritative contention result was observed.' }
    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*request=1 result=11 replay=0 ' 1 $Server)) { throw 'No sold-out authoritative contention result was observed.' }
    $SuccessCount = @(Select-String -LiteralPath $ServerLog -Pattern '\[Commerce\]\[Server\] PurchaseResult .*request=1 result=0 replay=0 ').Count
    $SoldOutCount = @(Select-String -LiteralPath $ServerLog -Pattern '\[Commerce\]\[Server\] PurchaseResult .*request=1 result=11 replay=0 ').Count
    if ($SuccessCount -ne 1 -or $SoldOutCount -ne 1) { throw "Expected exactly one successful and one sold-out purchase; success=$SuccessCount soldOut=$SoldOutCount." }
    $Assertions.ExactlyOnceContention = $true
    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*request=1 result=(0|11) replay=1 ' 2 $Server)) { throw 'Exact replay results were not returned from the bounded cache.' }
    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*request=3 result=4 replay=0 ' 2 $Server)) { throw 'Request gaps were not rejected.' }
    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*request=4 result=1 replay=0 ' 2 $Server)) { throw 'Wrong session nonces were not rejected.' }
    $Assertions.ReplayGapAndNonceRejection = $true
    if ((Select-String -LiteralPath $Client1Log -Pattern 'ForeignEconomyState|ClientMutationAccepted=1' -Quiet) -or (Select-String -LiteralPath $Client2Log -Pattern 'ForeignEconomyState|ClientMutationAccepted=1' -Quiet)) { throw 'A client reported an unauthorized mutation or foreign economy state.' }
    $Assertions.OwnerPrivacyAndNoClientMutation = $true

    Start-Sleep -Seconds $WarmupSeconds
    $FirstTime = [DateTime]::UtcNow
    $FirstCpu = $Server.Process.TotalProcessorTime.TotalSeconds
    $Samples = [System.Collections.Generic.List[object]]::new()
    $Deadline = $FirstTime.AddSeconds($SampleSeconds)
    while ([DateTime]::UtcNow -lt $Deadline) {
        Start-Sleep -Seconds 5
        if ((Get-State $Server) -ne 'Running') { throw 'Server exited during the performance sample.' }
        $Sample = Get-PerformanceSample $Server $FirstTime $FirstCpu
        $Samples.Add($Sample)
        $FirstTime = $Sample.NextTime
        $FirstCpu = $Sample.NextCpuSeconds
    }
    $PerformanceSamples = @($Samples | ForEach-Object { [ordered]@{ TimeUtc = $_.TimeUtc; CpuPercent = $_.CpuPercent; WorkingSetMiB = $_.WorkingSetMiB } })
    if ($PerformanceSamples.Count -lt 5) { throw 'Performance sample did not collect enough bounded-state observations.' }
    $CpuValues = @($PerformanceSamples | ForEach-Object { $_.CpuPercent })
    $MemoryValues = @($PerformanceSamples | ForEach-Object { $_.WorkingSetMiB })
    $CpuP95 = Get-Percentile $CpuValues 0.95
    $CpuP99 = Get-Percentile $CpuValues 0.99
    $MemoryGrowthMiB = ([double]($MemoryValues | Measure-Object -Maximum).Maximum - [double]($MemoryValues | Measure-Object -Minimum).Minimum)
    $BandwidthLines = @(Select-String -LiteralPath $ServerLog -Pattern '\[Day19PerformanceProbe\]\[Server\] Sample=.*ClientBandwidthMaxKiBps=([0-9]+)')
    $MaxBandwidth = 0
    foreach ($Line in $BandwidthLines) { if ($Line.Line -match 'ClientBandwidthMaxKiBps=([0-9]+)') { $MaxBandwidth = [Math]::Max($MaxBandwidth, [int]$Matches[1]) } }
    if ($CpuP95 -gt 33.3 -or $CpuP99 -gt 50.0 -or $MemoryGrowthMiB -gt 128.0 -or $MaxBandwidth -gt 512) {
        throw "Performance threshold failed: p95=$CpuP95 p99=$CpuP99 memoryGrowthMiB=$MemoryGrowthMiB maxClientBandwidthKiBps=$MaxBandwidth."
    }
    $Assertions.PerformanceBounded = $true

    $CrashPattern = 'Fatal error:|Assertion failed:|Unhandled Exception|Ensure condition failed'
    foreach ($Log in @($ServerLog, $Client1Log, $Client2Log)) {
        if ((Test-Path -LiteralPath $Log) -and (Select-String -LiteralPath $Log -Pattern $CrashPattern -Quiet)) { throw "Crash signature found in $Log." }
    }
    $Assertions.NoCrash = $true
    $Passed = $true
}
catch { $Failure = $_.Exception.Message }
finally {
    foreach ($Item in @($Owned | Sort-Object Name -Descending)) { Stop-Owned $Item }
    $ReportProcesses = @($Owned | ForEach-Object {
        $_.Process.Refresh()
        [ordered]@{ Name = $_.Name; ProcessId = $_.Process.Id; Arguments = $_.Arguments; State = Get-State $_; ExitCode = if ($_.Process.HasExited) { $_.Process.ExitCode } else { $null } }
    })
    $Report = [ordered]@{
        SchemaVersion = 1; Day = 19; Mode = $Mode; WorldPersistenceId = $WorldId; Port = $Port
        NetworkEmulation = [ordered]@{ LagMs = $NetworkLagMs; JitterMs = $NetworkJitterMs; PacketLossPercent = $PacketLossPercent }
        Performance = [ordered]@{ WarmupSeconds = $WarmupSeconds; SampleSeconds = $SampleSeconds; Samples = $PerformanceSamples }
        StartedUtc = $StartedAt.ToString('o'); FinishedUtc = [DateTime]::UtcNow.ToString('o')
        Assertions = $Assertions; Passed = $Passed; Failure = $Failure; Processes = $ReportProcesses
        Artifacts = [ordered]@{ ServerLog = $ServerLog; Client1Log = $Client1Log; Client2Log = $Client2Log; Report = $ReportPath }
    }
    $Report | ConvertTo-Json -Depth 12 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}
if (-not $Passed) { Write-Error "[Day19MultiplayerSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[Day19MultiplayerSmoke][$Mode] PASS"
exit 0
