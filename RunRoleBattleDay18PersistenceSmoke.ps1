[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [ValidateSet('Listen', 'Dedicated')] [string]$Mode,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [ValidateRange(1, 65535)] [int]$Port = 18918,
    [ValidateRange(30, 300)] [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$DayLabel = 'Day18'
$WorldId = 'Day18WorldFixture' + ([guid]::NewGuid().ToString('N'))
$ServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client2.log"
$RestartServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-RestartServer.log"
$IsolatedServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-IsolatedServer.log"
$ProviderMismatchServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-ProviderMismatchServer.log"
$ProviderMismatchClientLog = Join-Path $LogDirectory "$DayLabel-$Mode-ProviderMismatchClient.log"
$DuplicateLog = Join-Path $LogDirectory "$DayLabel-$Mode-Duplicate.log"
$ReportPath = Join-Path $ReportDirectory "$DayLabel-$Mode.json"
if (-not (Test-Path -LiteralPath $EditorExe)) { throw "UnrealEditor-Cmd.exe was not found at '$EditorExe'." }
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $RestartServerLog, $IsolatedServerLog, $ProviderMismatchServerLog, $ProviderMismatchClientLog, $DuplicateLog, $ReportPath)) { if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force } }

$Assertions = [ordered]@{}
$Owned = @()
$Passed = $false
$Failure = $null
$StartedAt = [DateTime]::UtcNow
function Get-State($Item) { try { $Item.Process.Refresh(); if ($Item.Process.HasExited) { return 'Exited' }; return 'Running' } catch { return 'Unavailable' } }
function Start-Owned([string]$Name, [string[]]$Arguments, [string]$LogPath) {
    $Process = Start-Process -FilePath $EditorExe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    $Item = [pscustomobject]@{ Name = $Name; Process = $Process; Arguments = $Arguments; LogPath = $LogPath }
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
        if ((Test-Path -LiteralPath $Path) -and @((Select-String -LiteralPath $Path -Pattern $Pattern)).Count -ge $Minimum) { return $true }
        if ($Item -and (Get-State $Item) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}
function Stop-Owned($Item) { if ($Item -and (Get-State $Item) -eq 'Running') { Stop-Process -Id $Item.Process.Id -Force -ErrorAction SilentlyContinue } }
function Server-Arguments([int]$ServerPort, [string]$ServerLogPath, [string]$ServerWorldId, [switch]$UseFixtureProvider, [switch]$ForceDedicated) {
    $Map = if ($Mode -eq 'Listen' -and -not $ForceDedicated) { '/Game/Maps/StartupMap?Role=Aura?listen' } else { '/Game/Maps/StartupMap' }
    $ArgumentsOut = @((Join-Path $ProjectRoot 'Aura.uproject'), $Map, '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-port=$ServerPort", '-RoleBattleDay18PersistenceSmoke', "-WorldPersistenceId=$ServerWorldId", "-abslog=$ServerLogPath")
    if ($UseFixtureProvider) { $ArgumentsOut += '-AuraPersistenceProvider=NULL' }
    $ArgumentsOut += if ($Mode -eq 'Listen' -and -not $ForceDedicated) { '-game' } else { '-server' }
    return $ArgumentsOut
}
try {
    & py (Join-Path $ProjectRoot 'Scripts\test_role_battle_days_18.py')
    if ($LASTEXITCODE -ne 0) { throw 'Focused Day 18 contracts failed.' }
    $Assertions.StaticContracts = $true

    $Server = Start-Owned 'Server' (Server-Arguments -ServerPort $Port -ServerLogPath $ServerLog -ServerWorldId $WorldId -UseFixtureProvider) $ServerLog
    if (-not (Wait-Pattern $ServerLog '\[WorldReadiness\] State=Ready' $Server)) { throw 'Initial server world readiness did not complete.' }
    if (-not (Wait-Pattern $ServerLog '\[Persistence\]\[World\] LoadedOnce=1' $Server)) { throw 'Initial world load did not complete exactly once.' }
    $Assertions.InitialWorldReady = $true

    $Client1 = Start-Owned 'Client1' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=SameDisplay?Role=Aura?AuraFixtureIdentity=Day18ProfileA", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-RoleBattleDay18PersistenceProbeClientA', "-abslog=$Client1Log") $Client1Log
    $Client2 = Start-Owned 'Client2' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=SameDisplay?Role=Aura?AuraFixtureIdentity=Day18ProfileB", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-RoleBattleDay18PersistenceProbeClientB', "-abslog=$Client2Log") $Client2Log
    if (-not (Wait-Count $ServerLog '\[Persistence\]\[Server\] ProfilePrepared=1 .*existing=0' 2 $Server)) { throw 'Two distinct fixture profiles did not prepare.' }
    $Assertions.DistinctProfilesPrepared = $true
    if (-not (Wait-Pattern $ServerLog '\[Commerce\]\[Session\] Rotated' $Server)) { throw 'Initial commerce sessions did not rotate.' }
    if (-not (Wait-Pattern $ServerLog '\[Day18PersistenceProbe\]\[Server\] FixtureReady=1' $Server)) { throw 'Day 18 transaction fixture did not become ready.' }
    $Assertions.TransactionFixtureReady = $true
    if (-not (Wait-Count $ServerLog '\[Commerce\]\[Server\] PurchaseResult .*result=0 replay=0 ' 2 $Server)) { throw 'Both profiles did not complete an authoritative purchase.' }
    if (-not (Select-String -LiteralPath $ServerLog -Pattern 'offer=market_health_potion' -Quiet)) { throw 'Profile A health-potion purchase was not observed.' }
    if (-not (Select-String -LiteralPath $ServerLog -Pattern 'offer=market_mana_potion' -Quiet)) { throw 'Profile B mana-potion purchase was not observed.' }
    $Assertions.IsolatedPurchases = $true
    if (-not (Wait-Count $ServerLog '\[Persistence\]\[Purchase\] Checkpoint=1 player=1 world=1' 2 $Server)) { throw 'Successful purchases did not durably checkpoint player and world state.' }
    $Assertions.PurchaseCheckpoint = $true
    $InitialNonce = $null
    foreach ($Line in @(Select-String -LiteralPath $ServerLog -Pattern '\[Commerce\]\[Session\] Rotated')) {
        if ($Line.Line -match 'nonce=([A-Fa-f0-9]{32})') { $InitialNonce = $Matches[1]; break }
    }
    if ([string]::IsNullOrEmpty($InitialNonce)) { throw 'Could not capture an old commerce session nonce for reconnect validation.' }

    $Duplicate = Start-Owned 'DuplicateProfile' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=AnotherDisplay?Role=Aura?AuraFixtureIdentity=Day18ProfileA", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$DuplicateLog") $DuplicateLog
    if (-not (Wait-Pattern $ServerLog 'already active' $Server)) { throw 'Duplicate authenticated profile was not rejected.' }
    $Assertions.DuplicateProfileRejected = $true
    Stop-Owned $Duplicate

    Stop-Owned $Client1; Stop-Owned $Client2; Stop-Owned $Server
    Start-Sleep -Seconds 2

    $RestartServer = Start-Owned 'RestartServer' (Server-Arguments -ServerPort ($Port + 1) -ServerLogPath $RestartServerLog -ServerWorldId $WorldId -UseFixtureProvider) $RestartServerLog
    if (-not (Wait-Pattern $RestartServerLog '\[Persistence\]\[World\] LoadedOnce=1 generation=' $RestartServer)) { throw 'Restart server did not load the prior world record.' }
    if (-not (Wait-Pattern $RestartServerLog 'population=[1-9][0-9]* merchants=[1-9][0-9]*' $RestartServer)) { throw 'Restart server did not restore population and merchant snapshot.' }
    $Assertions.WorldAndMerchantRestored = $true

    $Reconnect1 = Start-Owned 'Reconnect1' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:$($Port + 1)?PlayerName=SameDisplay?Role=Aura?AuraFixtureIdentity=Day18ProfileA", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-RoleBattleDay18PersistenceProbeClientA', '-RoleBattleDay18PersistenceStaleOnly', "-AuraRoleBattleDay18StaleNonce=$InitialNonce", "-abslog=$Client1Log") $Client1Log
    $Reconnect2 = Start-Owned 'Reconnect2' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:$($Port + 1)?PlayerName=SameDisplay?Role=Aura?AuraFixtureIdentity=Day18ProfileB", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-RoleBattleDay18PersistenceProbeClientB', '-RoleBattleDay18PersistenceStaleOnly', "-AuraRoleBattleDay18StaleNonce=$InitialNonce", "-abslog=$Client2Log") $Client2Log
    if (-not (Wait-Count $RestartServerLog '\[Persistence\]\[Server\] ProfilePrepared=1 .*existing=1' 2 $RestartServer)) { throw 'Reconnect did not restore both profile records.' }
    $Assertions.ReconnectIsolation = $true
    $PreparedLines = @(Select-String -LiteralPath $RestartServerLog -Pattern '\[Persistence\]\[Server\] ProfilePrepared=1')
    $Hashes = @($PreparedLines | ForEach-Object { if ($_.Line -match 'identityHash=([^ ]+)') { $Matches[1] } } | Select-Object -Unique)
    if ($Hashes.Count -lt 2) { throw 'The two profile identities did not remain distinct after reconnect.' }
    $Assertions.IdentityHashIsolation = $true
    if (-not (Wait-Count $RestartServerLog '\[Commerce\]\[Session\] Rotated' 2 $RestartServer)) { throw 'Reconnect did not rotate new commerce sessions.' }
    $Assertions.ReconnectNonceRotation = $true
    if (-not (Wait-Count $RestartServerLog '\[Persistence\]\[Profile\] Applied .* balance=(75|70) ' 2 $RestartServer)) { throw 'Reconnect did not apply the two isolated post-purchase balances.' }
    $BalanceLines = @(Select-String -LiteralPath $RestartServerLog -Pattern '\[Persistence\]\[Profile\] Applied .* balance=(75|70) ')
    if (-not ($BalanceLines.Line -match 'balance=75 ') -or -not ($BalanceLines.Line -match 'balance=70 ')) { throw 'Reconnect balance records were not isolated to 75 and 70.' }
    $Assertions.BalanceIsolation = $true
    if (-not (Wait-Count $RestartServerLog '\[Commerce\]\[Server\] PurchaseResult .*request=999 result=1 replay=0 ' 2 $RestartServer)) { throw 'The old commerce nonce was accepted after reconnect.' }
    $Assertions.OldNonceRejected = $true

    $IsolatedWorldId = 'Day18IsolatedWorld' + ([guid]::NewGuid().ToString('N'))
    $IsolatedServer = Start-Owned 'IsolatedServer' (Server-Arguments -ServerPort ($Port + 2) -ServerLogPath $IsolatedServerLog -ServerWorldId $IsolatedWorldId -UseFixtureProvider) $IsolatedServerLog
    if (-not (Wait-Pattern $IsolatedServerLog '\[Persistence\]\[World\] Configured WorldPersistenceId=' $IsolatedServer)) { throw 'Second world instance did not configure its own world ID.' }
    if (-not (Wait-Pattern $IsolatedServerLog '\[WorldReadiness\] State=Ready' $IsolatedServer)) { throw 'Second world instance did not reach readiness.' }
    $WorldPattern = [regex]::Escape($WorldId)
    if ((Select-String -LiteralPath $IsolatedServerLog -Pattern $WorldPattern -Quiet)) { throw 'World identity leaked between isolated server instances.' }
    $Assertions.WorldIdIsolation = $true

    $ProviderServer = Start-Owned 'ProviderMismatchServer' (Server-Arguments -ServerPort ($Port + 3) -ServerLogPath $ProviderMismatchServerLog -ServerWorldId ($WorldId + 'ProviderMismatch') -ForceDedicated) $ProviderMismatchServerLog
    if (-not (Wait-Pattern $ProviderMismatchServerLog '\[Persistence\]\[World\] Configured WorldPersistenceId=.* provider=Steam' $ProviderServer)) { throw 'Strict provider server did not retain the configured Steam provider.' }
    if (-not (Wait-Pattern $ProviderMismatchServerLog '\[WorldReadiness\] State=Ready' $ProviderServer)) { throw 'Strict provider server did not reach readiness.' }
    $ProviderMismatchClient = Start-Owned 'ProviderMismatchClient' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:$($Port + 3)?PlayerName=ProviderMismatch?Role=Aura?AuraFixtureIdentity=Day18ProfileA?AuraFixtureProvider=EOS", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$ProviderMismatchClientLog") $ProviderMismatchClientLog
    if (-not (Wait-Pattern $ProviderMismatchServerLog 'Identity provider mismatch' $ProviderServer)) { throw 'Provider mismatch was not rejected by the authority.' }
    $Assertions.ProviderMismatchRejected = $true
    Stop-Owned $ProviderMismatchClient; Stop-Owned $ProviderServer

    foreach ($Log in @($ServerLog, $RestartServerLog, $IsolatedServerLog, $ProviderMismatchServerLog, $DuplicateLog)) {
        if ((Test-Path -LiteralPath $Log) -and (Select-String -LiteralPath $Log -Pattern 'Fatal error:|Assertion failed:|Unhandled Exception|Ensure condition failed' -Quiet)) { throw "Crash signature found in $Log." }
    }
    $Assertions.NoCrash = $true
    $Passed = $true
}
catch { $Failure = $_.Exception.Message }
finally {
    foreach ($Item in @($Owned | Sort-Object Name -Descending)) { Stop-Owned $Item }
    $ReportProcesses = @($Owned | ForEach-Object { $_.Process.Refresh(); [ordered]@{ Name = $_.Name; ProcessId = $_.Process.Id; Arguments = $_.Arguments; State = Get-State $_; ExitCode = if ($_.Process.HasExited) { $_.Process.ExitCode } else { $null } } })
    $Report = [ordered]@{ SchemaVersion = 1; Day = 18; Mode = $Mode; WorldPersistenceId = $WorldId; Port = $Port; StartedUtc = $StartedAt.ToString('o'); FinishedUtc = [DateTime]::UtcNow.ToString('o'); Assertions = $Assertions; Passed = $Passed; Failure = $Failure; Processes = $ReportProcesses; Artifacts = [ordered]@{ ServerLog = $ServerLog; RestartServerLog = $RestartServerLog; Client1Log = $Client1Log; Client2Log = $Client2Log; IsolatedServerLog = $IsolatedServerLog; ProviderMismatchServerLog = $ProviderMismatchServerLog; ProviderMismatchClientLog = $ProviderMismatchClientLog; DuplicateLog = $DuplicateLog; Report = $ReportPath } }
    $Report | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}
if (-not $Passed) { Write-Error "[Day18PersistenceSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[Day18PersistenceSmoke][$Mode] PASS"
exit 0
