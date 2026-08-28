[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [ValidateSet('Listen', 'Dedicated')] [string]$Mode,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [ValidateRange(1, 65535)] [int]$Port = 17916,
    [ValidateRange(10, 300)] [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$DayLabel = 'Day16'
$ServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client2.log"
$ReportPath = Join-Path $ReportDirectory "$DayLabel-$Mode.json"
$RunId = [Guid]::NewGuid().ToString('N')
$FixtureRoot = Join-Path $ProjectRoot "Saved\RoleBattleDay16-$Mode-$RunId"
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
try {
    & py (Join-Path $ProjectRoot 'Scripts\test_role_battle_days_16.py')
    if ($LASTEXITCODE -ne 0) { throw 'Focused Day 16 contracts failed.' }
    $Assertions.StaticContracts = $true

    $ServerUserDir = Join-Path $FixtureRoot 'Server'
    $Client1UserDir = Join-Path $FixtureRoot 'Client1'
    $Client2UserDir = Join-Path $FixtureRoot 'Client2'
    foreach ($Directory in @($ServerUserDir, $Client1UserDir, $Client2UserDir)) { New-Item -ItemType Directory -Path $Directory -Force | Out-Null }

    $Map = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?Role=Aura?listen' } else { '/Game/Maps/StartupMap' }
    $ServerArgs = @((Join-Path $ProjectRoot 'Aura.uproject'), $Map, '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-port=$Port", '-RoleBattleDay16NetworkProbe', '-WorldPersistenceId=RoleBattleDay16', '-AuraPersistenceProvider=NULL', '-SaveToUserDir', "-UserDir=$ServerUserDir", "-abslog=$ServerLog")
    $ServerArgs += if ($Mode -eq 'Listen') { '-game' } else { '-server' }
    $Server = Start-Owned 'Server' $ServerArgs
    if (-not (Wait-Pattern $ServerLog '\[WorldReadiness\] State=Ready' $Server)) { throw 'Server world readiness did not complete.' }
    $Assertions.ServerWorldReady = $true

    $Client1 = Start-Owned 'Client1' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=Day16Client1?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-ExecCmds=GrantDevelopmentEconomy health_potion Quantity=99 CurrencyAmount=999', '-SaveToUserDir', "-UserDir=$Client1UserDir", "-abslog=$Client1Log")
    $Client2 = Start-Owned 'Client2' @((Join-Path $ProjectRoot 'Aura.uproject'), "127.0.0.1:${Port}?PlayerName=Day16Client2?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', '-ExecCmds=GrantDevelopmentEconomy health_potion Quantity=99 CurrencyAmount=999', '-SaveToUserDir', "-UserDir=$Client2UserDir", "-abslog=$Client2Log")
    if (-not (Wait-Pattern $ServerLog '\[Day16NetworkProbe\]\[Server\] Passed=1' $Server)) { throw 'Day 16 server owner/respawn probe failed or did not run.' }
    $Assertions.ServerOwnerOnlyAndRespawn = $true
    if (-not (Wait-Pattern $Client1Log '\[Economy\]\[Client\] Player=Day16Client1 Currency replicated' $Client1)) { throw 'Client1 did not receive its owner-only currency state.' }
    if (-not (Wait-Pattern $Client1Log '\[Economy\]\[Client\] Player=Day16Client1 Inventory replicated' $Client1)) { throw 'Client1 did not receive its owner-only inventory state.' }
    if (-not (Wait-Pattern $Client2Log '\[Economy\]\[Client\] Player=Day16Client2 Currency replicated' $Client2)) { throw 'Client2 did not receive its owner-only currency state.' }
    if (-not (Wait-Pattern $Client2Log '\[Economy\]\[Client\] Player=Day16Client2 Inventory replicated' $Client2)) { throw 'Client2 did not receive its owner-only inventory state.' }
    $Assertions.Client1OwnerState = $true
    $Assertions.Client2OwnerState = $true
    if (Select-String -LiteralPath $Client1Log -Pattern '\[Economy\]\[Client\] Player=Day16Client2 (Currency|Inventory) replicated' -Quiet) { throw 'Client1 received Client2 economy state.' }
    if (Select-String -LiteralPath $Client2Log -Pattern '\[Economy\]\[Client\] Player=Day16Client1 (Currency|Inventory) replicated' -Quiet) { throw 'Client2 received Client1 economy state.' }
    if (Select-String -LiteralPath $Client1Log -Pattern '\[Economy\]\[DevGrant\] Applied' -Quiet) { throw 'Client1 executed the authority-only development grant.' }
    if (Select-String -LiteralPath $Client2Log -Pattern '\[Economy\]\[DevGrant\] Applied' -Quiet) { throw 'Client2 executed the authority-only development grant.' }
    $Assertions.ObserverPrivacy = $true
    $Assertions.RemoteGrantRejected = $true
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
    $Report = [ordered]@{ SchemaVersion = 1; Day = 16; Mode = $Mode; Port = $Port; StartedUtc = $StartedAt.ToString('o'); FinishedUtc = [DateTime]::UtcNow.ToString('o'); Assertions = $Assertions; Passed = $Passed; Failure = $Failure; Processes = $ReportProcesses; Artifacts = [ordered]@{ ServerLog = $ServerLog; Client1Log = $Client1Log; Client2Log = $Client2Log; Report = $ReportPath }; IsolatedFixtureRemoved = $FixtureRemoved }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}
if (-not $Passed) { Write-Error "[Day16NetworkSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[Day16NetworkSmoke][$Mode] PASS"
exit 0
