[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [ValidateSet('10', '11', '12', '13', '14', '15')] [string]$Day,
    [Parameter(Mandatory = $true)] [ValidateSet('Listen', 'Dedicated')] [string]$Mode,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [ValidateRange(1, 65535)] [int]$Port = 17910,
    [ValidateRange(10, 300)] [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$DayLabel = "Day$Day"
$ServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client2.log"
$ReportPath = Join-Path $ReportDirectory "$DayLabel-$Mode.json"
$RunId = [Guid]::NewGuid().ToString('N')
$WorldPersistenceId = "RoleBattleDay${Day}-$Mode-$RunId"
if (-not (Test-Path -LiteralPath $EditorExe)) { throw "UnrealEditor-Cmd.exe was not found at '$EditorExe'." }
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $ReportPath)) { if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force } }

$Owned = @()
$Assertions = [ordered]@{}
$Passed = $false
$Failure = $null
function State($Item) { try { $Item.Process.Refresh(); if ($Item.Process.HasExited) { return 'Exited' }; return 'Running' } catch { return 'Unavailable' } }
function Start-Owned([string]$Name, [string[]]$Arguments) {
    $Process = Start-Process -FilePath $EditorExe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    $Item = [pscustomobject]@{ Name=$Name; Process=$Process; Arguments=$Arguments; Status='Running'; ExitCode=$null }
    $script:Owned += $Item
    return $Item
}
function Wait-Pattern([string]$Path, [string]$Pattern, $Item) {
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if ((Test-Path -LiteralPath $Path) -and (Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)) { return $true }
        if ($Item -and (State $Item) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}
function Wait-PatternCount([string]$Path, [string]$Pattern, [int]$MinimumCount, $Item) {
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (Test-Path -LiteralPath $Path) {
            $Count = @(Select-String -LiteralPath $Path -Pattern $Pattern).Count
            if ($Count -ge $MinimumCount) { return $true }
        }
        if ($Item -and (State $Item) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}

try {
    $StaticSuite = if ([int]$Day -ge 13) { 'Scripts\test_role_battle_days_13_15.py' } else { 'Scripts\test_role_battle_days_10_12.py' }
    & python (Join-Path $ProjectRoot $StaticSuite)
    if ($LASTEXITCODE -ne 0) { throw "Focused Day $Day contracts failed." }
    $Assertions.StaticContracts = $true
    $Map = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?Role=Aura?listen' } else { '/Game/Maps/StartupMap' }
    $ServerArgs = @((Join-Path $ProjectRoot 'Aura.uproject'), $Map, '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-port=$Port", "-WorldPersistenceId=$WorldPersistenceId", '-AuraPersistenceProvider=NULL', "-RoleBattleDay${Day}NetworkProbe", "-abslog=$ServerLog")
    $ServerArgs += if ($Mode -eq 'Listen') { '-game' } else { '-server' }
    $Server = Start-Owned 'Server' $ServerArgs
    if (-not (Wait-Pattern $ServerLog '\[WorldReadiness\] State=Ready' $Server)) { throw 'Coordinated world-readiness evidence is missing.' }
    $Assertions.ServerWorldCoordinatorReady = $true
    if (-not (Wait-Pattern $ServerLog "\[Day${Day}NetworkProbe\]\[Server\] Passed=1" $Server)) { throw "Day $Day authoritative probe failed or did not run." }
    $Assertions.ServerDayProbePassed = $true
    $Client1 = Start-Owned 'Client1' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=Day${Day}Client1?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$Client1Log")
    $Client2 = Start-Owned 'Client2' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=Day${Day}Client2?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$Client2Log")
    if (-not (Wait-Pattern $Client1Log '\[BattleDirector\]\[Client\] Replicated director observed' $Client1)) { throw 'Client 1 did not observe the replicated battle director.' }
    if (-not (Wait-Pattern $Client2Log '\[BattleDirector\]\[Client\] Replicated director observed' $Client2)) { throw 'Client 2 did not observe the replicated battle director.' }
    $Assertions.Client1ReplicatedDirector = $true
    $Assertions.Client2LateJoinReplicatedDirector = $true
    if (-not (Wait-PatternCount $Client1Log '\[Civilian\]\[Population\] Replicated member=' 3 $Client1)) { throw 'Client 1 did not receive all three population member states.' }
    if (-not (Wait-PatternCount $Client2Log '\[Civilian\]\[Population\] Replicated member=' 3 $Client2)) { throw 'Client 2 did not receive all three late-join population member states.' }
    $Assertions.Client1PopulationReplicated = $true
    $Assertions.Client2LateJoinPopulationReplicated = $true
    $CrashPattern = 'Fatal error:|Assertion failed:|Unhandled Exception|Ensure condition failed'
    foreach ($Log in @($ServerLog, $Client1Log, $Client2Log)) { if ((Test-Path -LiteralPath $Log) -and (Select-String -LiteralPath $Log -Pattern $CrashPattern -Quiet)) { throw "Crash signature found in $Log." } }
    $Assertions.NoCrash = $true
    $Passed = $true
}
catch { $Failure = $_.Exception.Message }
finally {
    foreach ($Item in @($Owned | Sort-Object Name -Descending)) {
        if ((State $Item) -eq 'Running') { Stop-Process -Id $Item.Process.Id -Force -ErrorAction SilentlyContinue }
        try { $Item.ExitCode = $Item.Process.ExitCode } catch { $Item.ExitCode = $null }
        $Item.Status = 'Stopped'
    }
    $Report = [ordered]@{ SchemaVersion=1; Day=$Day; Mode=$Mode; Port=$Port; WorldPersistenceId=$WorldPersistenceId; Assertions=$Assertions; Passed=$Passed; Failure=$Failure; Artifacts=[ordered]@{ServerLog=$ServerLog; Client1Log=$Client1Log; Client2Log=$Client2Log; Report=$ReportPath} }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}
if (-not $Passed) { Write-Error "[$DayLabel`NetworkSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[$DayLabel`NetworkSmoke][$Mode] PASS"
exit 0
