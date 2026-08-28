[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [ValidateSet('Listen', 'Dedicated')] [string]$Mode,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [ValidateRange(1, 65535)] [int]$Port = 17917,
    [ValidateRange(10, 300)] [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$DayLabel = 'Day17'
$ServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client2.log"
$ReportPath = Join-Path $ReportDirectory "$DayLabel-$Mode.json"
$RunId = [Guid]::NewGuid().ToString('N')
$FixtureRoot = Join-Path $ProjectRoot "Saved\RoleBattleDay17-$Mode-$RunId"
$WorldPersistenceId = "RoleBattleDay17-$Mode-$RunId"
if (-not (Test-Path -LiteralPath $EditorExe)) { throw "UnrealEditor-Cmd.exe was not found at '$EditorExe'." }
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $ReportPath)) { if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force } }

$Assertions = [ordered]@{}
$Passed = $false
$Failure = $null
$Owned = @()
$StartedAt = [DateTime]::UtcNow
function Get-State($Item) {
    try { $Item.Process.Refresh(); if ($Item.Process.HasExited) { return 'Exited' }; return 'Running' }
    catch { return 'Unavailable' }
}
function Start-Owned([string]$Name, [string[]]$Arguments) {
    $Process = Start-Process -FilePath $EditorExe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    $Item = [pscustomobject]@{ Name = $Name; Process = $Process; Arguments = $Arguments }
    $script:Owned += $Item
    return $Item
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
try {
    & py (Join-Path $ProjectRoot 'Scripts\test_role_battle_days_17.py')
    if ($LASTEXITCODE -ne 0) { throw 'Focused Day 17 contracts failed.' }
    $Assertions.StaticContracts = $true

    $ServerUserDir = Join-Path $FixtureRoot 'Server'
    $Client1UserDir = Join-Path $FixtureRoot 'Client1'
    $Client2UserDir = Join-Path $FixtureRoot 'Client2'
    foreach ($Directory in @($ServerUserDir, $Client1UserDir, $Client2UserDir)) { New-Item -ItemType Directory -Path $Directory -Force | Out-Null }

    $Map = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?Role=Aura?listen' } else { '/Game/Maps/StartupMap' }
    $ServerArgs = @((Join-Path $ProjectRoot 'Aura.uproject'), $Map, '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-port=$Port", '-RoleBattleDay17NetworkProbe', "-WorldPersistenceId=$WorldPersistenceId", '-AuraPersistenceProvider=NULL', '-SaveToUserDir', "-UserDir=$ServerUserDir", "-abslog=$ServerLog")
    $ServerArgs += if ($Mode -eq 'Listen') { '-game' } else { '-server' }
    $Server = Start-Owned 'Server' $ServerArgs
    if (-not (Wait-Pattern $ServerLog '\[WorldReadiness\] State=Ready' $Server)) { throw 'Server world readiness did not complete.' }
    $Assertions.ServerWorldReady = $true

    $Client1 = Start-Owned 'Client1' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=Day17Client1?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-RoleBattleDay17NetworkProbeClient', '-SaveToUserDir', "-UserDir=$Client1UserDir", "-abslog=$Client1Log")
    $Client2 = Start-Owned 'Client2' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=Day17Client2?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-RoleBattleDay17NetworkProbeClient', '-SaveToUserDir', "-UserDir=$Client2UserDir", "-abslog=$Client2Log")
    if (-not (Wait-Pattern $ServerLog '\[Day17NetworkProbe\]\[Server\] FixtureReady=1' $Server)) { throw 'Day 17 merchant fixture did not become ready.' }
    $Assertions.MerchantFixtureReady = $true
    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*result=0 replay=0 ' 1 $Server)) { throw 'No successful authoritative purchase was observed.' }
    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*result=11 replay=0 ' 1 $Server)) { throw 'No sold-out contention result was observed.' }
    $SuccessCount = @(Select-String -LiteralPath $ServerLog -Pattern '\[Commerce\]\[Server\] PurchaseResult .*result=0 replay=0 ').Count
    $SoldOutCount = @(Select-String -LiteralPath $ServerLog -Pattern '\[Commerce\]\[Server\] PurchaseResult .*result=11 replay=0 ').Count
    if ($SuccessCount -ne 1 -or $SoldOutCount -ne 1) { throw "Expected exactly one success and one sold-out result; success=$SuccessCount soldOut=$SoldOutCount." }
    $Assertions.ExactlyOnceContention = $true
    foreach ($Client in @(@{ Item = $Client1; Log = $Client1Log; Name = 'Day17Client1' }, @{ Item = $Client2; Log = $Client2Log; Name = 'Day17Client2' })) {
        if (-not (Wait-Pattern $Client.Log '\[Day17NetworkProbe\]\[Client\] Submitted request=1' $Client.Item)) { throw "$($Client.Name) did not submit a purchase." }
        if (-not (Wait-Count $Client.Log '\[Commerce\]\[Client\] PurchaseResult request=1 ' 2 $Client.Item)) { throw "$($Client.Name) did not receive the original result and exact replay result." }
        if (-not (Wait-Pattern $Client.Log '\[Commerce\]\[Client\] MerchantPresentation .*available=0' $Client.Item)) { throw "$($Client.Name) did not observe merchant UI closure." }
    }
    $Assertions.ClientResultCorrelation = $true
    $Assertions.MerchantDeathUiClosure = $true
    if (Select-String -LiteralPath $Client1Log -Pattern '\[Economy\]\[Client\] Player=Day17Client2 (Currency|Inventory) replicated' -Quiet) { throw 'Client1 received foreign wallet/inventory state.' }
    if (Select-String -LiteralPath $Client2Log -Pattern '\[Economy\]\[Client\] Player=Day17Client1 (Currency|Inventory) replicated' -Quiet) { throw 'Client2 received foreign wallet/inventory state.' }
    $Assertions.NonOwnerPrivacy = $true
    $CrashPattern = 'Fatal error:|Assertion failed:|Unhandled Exception|Ensure condition failed'
    foreach ($Log in @($ServerLog, $Client1Log, $Client2Log)) { if ((Test-Path -LiteralPath $Log) -and (Select-String -LiteralPath $Log -Pattern $CrashPattern -Quiet)) { throw "Crash signature found in $Log." } }
    $Assertions.NoCrash = $true
    $Passed = $true
}
catch { $Failure = $_.Exception.Message }
finally {
    foreach ($Item in @($Owned | Sort-Object Name -Descending)) {
        if ((Get-State $Item) -eq 'Running') { Stop-Process -Id $Item.Process.Id -Force -ErrorAction SilentlyContinue }
    }
    $ReportProcesses = @($Owned | ForEach-Object {
        $_.Process.Refresh()
        [ordered]@{ Name = $_.Name; ProcessId = $_.Process.Id; Arguments = $_.Arguments; State = Get-State $_; ExitCode = if ($_.Process.HasExited) { $_.Process.ExitCode } else { $null } }
    })
    $FixtureRemoved = $false
    $ResolvedFixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
    $ResolvedSavedRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved'))
    if ($ResolvedFixtureRoot.StartsWith($ResolvedSavedRoot, [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $ResolvedFixtureRoot)) {
        Remove-Item -LiteralPath $ResolvedFixtureRoot -Recurse -Force
        $FixtureRemoved = -not (Test-Path -LiteralPath $ResolvedFixtureRoot)
    }
    $Report = [ordered]@{ SchemaVersion = 1; Day = 17; Mode = $Mode; Port = $Port; WorldPersistenceId = $WorldPersistenceId; StartedUtc = $StartedAt.ToString('o'); FinishedUtc = [DateTime]::UtcNow.ToString('o'); Assertions = $Assertions; Passed = $Passed; Failure = $Failure; Processes = $ReportProcesses; Artifacts = [ordered]@{ ServerLog = $ServerLog; Client1Log = $Client1Log; Client2Log = $Client2Log; Report = $ReportPath }; IsolatedFixtureRemoved = $FixtureRemoved }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}
if (-not $Passed) { Write-Error "[Day17NetworkSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[Day17NetworkSmoke][$Mode] PASS"
exit 0
