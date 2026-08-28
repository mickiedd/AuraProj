[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('8', '9')]
    [string]$Day,
    [Parameter(Mandatory = $true)]
    [ValidateSet('Listen', 'Dedicated')]
    [string]$Mode,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [ValidateRange(1, 65535)] [int]$Port = 17890,
    [ValidateRange(10, 300)] [int]$TimeoutSeconds = 90
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'Aura.uproject'
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$DayLabel = "Day0$Day"
$ServerLog = Join-Path $LogDirectory "$DayLabel-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "$DayLabel-$Mode-Client2.log"
$ReportPath = Join-Path $ReportDirectory "$DayLabel-$Mode.json"

if (-not (Test-Path -LiteralPath $EditorExe)) { throw "UnrealEditor-Cmd.exe was not found at '$EditorExe'." }
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$OwnedProcesses = @()
$Assertions = [ordered]@{}
$Passed = $false
$Failure = $null
$StartedUtc = [DateTime]::UtcNow.ToString('o')

function Get-State([System.Diagnostics.Process]$Process) {
    if ($null -eq $Process) { return 'NotStarted' }
    try { $Process.Refresh(); if ($Process.HasExited) { return 'Exited' }; return 'Running' } catch { return 'Unavailable' }
}

function Start-Owned([string]$Name, [string[]]$Arguments) {
    $Process = Start-Process -FilePath $EditorExe -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    $Item = [pscustomobject]@{ Name=$Name; Process=$Process; Arguments=@($Arguments); Status='Running'; ExitCode=$null }
    $script:OwnedProcesses += $Item
    return $Item
}

function Stop-Owned([pscustomobject]$Item) {
    if ($null -eq $Item -or $null -eq $Item.Process) { return }
    if ((Get-State $Item.Process) -eq 'Running') { Stop-Process -Id $Item.Process.Id -Force -ErrorAction SilentlyContinue }
    try { $Item.ExitCode = $Item.Process.ExitCode } catch { $Item.ExitCode = $null }
    $Item.Status = if ((Get-State $Item.Process) -eq 'Running') { 'TeardownTimeout' } else { 'Stopped' }
}

function Test-Pattern([string]$Path, [string]$Pattern) {
    return (Test-Path -LiteralPath $Path) -and [bool](Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)
}

function Test-PatternAbsent([string]$Path, [string]$Pattern) {
    return (Test-Path -LiteralPath $Path) -and -not [bool](Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)
}

function Wait-Pattern([string]$Path, [string]$Pattern, [pscustomobject]$Process) {
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (Test-Pattern $Path $Pattern) { return $true }
        if ($null -ne $Process -and (Get-State $Process.Process) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}

try {
    $ServerMap = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?Role=Aura?listen' } else { '/Game/Maps/StartupMap' }
    $ServerArgs = @($ProjectFile, $ServerMap, '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-port=$Port", "-WorldPersistenceId=RoleBattleDay${Day}", '-AuraPersistenceProvider=NULL', "-RoleBattleDay${Day}NetworkProbe", "-abslog=$ServerLog")
    if ($Mode -eq 'Dedicated') { $ServerArgs += '-server' } else { $ServerArgs += '-game' }
    $Server = Start-Owned 'Server' $ServerArgs

    $ServerPattern = if ($Day -eq '8') {
        '\[Day8NetworkProbe\]\[Server\] Spawned Civilian=.*Role=Civilian IdentityValid=1'
    } else {
        '\[Population\]\[Finalize\].*liveMembers=3 success=1'
    }
    if (-not (Wait-Pattern $ServerLog $ServerPattern $Server)) { throw "Day $Day server initialization gate failed." }
    $Assertions.ServerInitialization = $true

    if ($Day -eq '8') {
        if (-not (Wait-Pattern $ServerLog '\[Civilian\]\[Init\].*ASCOwner=.*Avatar=.*identity=Faction\.Civilian/Control\.CivilianAI/Combat\.Civilian')) { throw 'Day 8 server Civilian identity/ASC initialization evidence is missing.' }
        if (-not (Wait-Pattern $ServerLog '\[RoleGrant\]\[Server\] Role=Civilian Required=0 OwnedSpecs=0')) { throw 'Day 8 server empty offensive loadout evidence is missing.' }
        $Assertions.ServerCivilianIdentity = $true
        $Assertions.ServerEmptyOffensiveLoadout = $true
    } else {
        foreach ($SlotIndex in 0..2) {
            if (-not (Wait-Pattern $ServerLog "\[Population\]\[Spawn\].*slot=$SlotIndex member=MarketCivilians:$SlotIndex actor=.* role=Civilian" $Server)) { throw "Day 9 server did not commit canonical slot $SlotIndex." }
        }
        if (-not (Wait-Pattern $ServerLog '\[Population\]\[Volume\] Registered id=MarketCiviliansVolume')) { throw 'Day 9 spawn volume registration evidence is missing.' }
        if (-not (Wait-Pattern $ServerLog '\[Population\]\[Probe\] DuplicateFinalize generation=1 liveMembers=3 stable=1 result=1' $Server)) { throw 'Day 9 duplicate initialization probe did not remain stable.' }
        if (-not (Test-PatternAbsent $ServerLog '\[Population\]\[(Respawn|Refill)\]')) { throw 'Day 9 unexpectedly entered a respawn/refill path.' }
        $Assertions.ServerCanonicalSlots = '3/3'
        $Assertions.ServerVolumeRegistration = $true
        $Assertions.ServerDuplicateInitialization = $true
        $Assertions.NoDay9RespawnOrRefill = $true
    }

    $Client1 = Start-Owned 'Client1' @($ProjectFile, "127.0.0.1:${Port}?PlayerName=Day${Day}Client1?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$Client1Log")
    if ($Day -eq '8') {
        if (-not (Wait-Pattern $Client1Log '\[RolePresentation\]\[Client\].*Actor=AuraCivilian_0 Role=Civilian Weapon=None' $Client1)) { throw 'Day 8 Client1 did not reconstruct empty Civilian presentation.' }
        if (-not (Wait-Pattern $Client1Log '\[Day6Role\]\[Client\].*Actor=AuraCivilian_0 Role=Civilian Entity=Entity\.AmbientNPC.*Interaction=Interaction\.Civilian' $Client1)) { throw 'Day 8 Client1 did not reconstruct the full role presentation state.' }
        $Assertions.Client1Presentation = $true
    } else {
        foreach ($SlotIndex in 0..2) {
            $ExpectedMerchant = if ($SlotIndex -eq 1) { 'MarketMerchant' } else { 'None' }
            if (-not (Wait-Pattern $Client1Log "\[Civilian\]\[Population\] Replicated member=MarketCivilians:$SlotIndex population=MarketCivilians slot=$SlotIndex work=Observer zone=Market merchant=$ExpectedMerchant" $Client1)) { throw "Day 9 Client1 did not reconstruct canonical slot $SlotIndex." }
        }
        if (-not (Wait-Pattern $Client1Log '\[Day6Role\]\[Client\].*Actor=AuraCivilian_.*Role=Civilian Entity=Entity\.AmbientNPC.*Interaction=Interaction\.Civilian' $Client1)) { throw 'Day 9 Client1 did not reconstruct Civilian role state.' }
        $Assertions.Client1CanonicalMembers = '3/3'
    }

    $Client2 = Start-Owned 'Client2' @($ProjectFile, "127.0.0.1:${Port}?PlayerName=Day${Day}Client2?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$Client2Log")
    if ($Day -eq '8') {
        if (-not (Wait-Pattern $Client2Log '\[RolePresentation\]\[Client\].*Actor=AuraCivilian_0 Role=Civilian Weapon=None' $Client2)) { throw 'Day 8 Client2 did not reconstruct empty Civilian presentation.' }
        if (-not (Wait-Pattern $Client2Log '\[Day6Role\]\[Client\].*Actor=AuraCivilian_0 Role=Civilian Entity=Entity\.AmbientNPC.*Interaction=Interaction\.Civilian' $Client2)) { throw 'Day 8 Client2 did not reconstruct the full role presentation state.' }
        $Assertions.Client2Presentation = $true
    } else {
        foreach ($SlotIndex in 0..2) {
            $ExpectedMerchant = if ($SlotIndex -eq 1) { 'MarketMerchant' } else { 'None' }
            if (-not (Wait-Pattern $Client2Log "\[Civilian\]\[Population\] Replicated member=MarketCivilians:$SlotIndex population=MarketCivilians slot=$SlotIndex work=Observer zone=Market merchant=$ExpectedMerchant" $Client2)) { throw "Day 9 Client2 did not reconstruct late-join slot $SlotIndex." }
        }
        if (-not (Wait-Pattern $Client2Log '\[Day6Role\]\[Client\].*Actor=AuraCivilian_.*Role=Civilian Entity=Entity\.AmbientNPC.*Interaction=Interaction\.Civilian' $Client2)) { throw 'Day 9 Client2 did not reconstruct late-join Civilian role state.' }
        $Assertions.Client2LateJoinCanonicalMembers = '3/3'
    }
    $Assertions.Client1Replication = $true
    $Assertions.Client2Replication = $true

    $ProbeLogs = @($ServerLog, $Client1Log, $Client2Log)
    $CrashPattern = 'Fatal error:|Assertion failed:|Unhandled Exception|Ensure condition failed'
    if (@($ProbeLogs | Where-Object { Test-Pattern $_ $CrashPattern }).Count -gt 0) {
        throw 'A Civilian network process reported a fatal/crash signature.'
    }
    if (Test-Pattern $ServerLog '\[Population\]\[Finalize\] Failed|\[Day8NetworkProbe\]\[Server\] FAIL') {
        throw 'The server reported a population or Day 8 probe failure.'
    }
    foreach ($Owned in $OwnedProcesses) {
        if ((Get-State $Owned.Process) -ne 'Running') {
            throw "$($Owned.Name) exited before network assertions completed."
        }
    }
    $Assertions.NoCrashOrUnexpectedServerFailure = $true
    $Passed = $true
}
catch { $Failure = $_.Exception.Message }
finally {
    foreach ($Item in @($OwnedProcesses | Sort-Object Name -Descending)) { Stop-Owned $Item }
    if (@($OwnedProcesses | Where-Object { $_.Status -eq 'TeardownTimeout' }).Count -gt 0) {
        $Passed = $false
        $Failure = if ($Failure) { "$Failure Owned process teardown timed out." } else { 'Owned process teardown timed out.' }
    }
    $Required = @($ServerLog, $Client1Log, $Client2Log)
    $Missing = @($Required | Where-Object { -not (Test-Path -LiteralPath $_) })
    if ($Missing.Count -gt 0) { $Passed = $false; $Failure = if ($Failure) { "$Failure Missing: $($Missing -join ', ')" } else { "Missing: $($Missing -join ', ')" } }
    $Report = [ordered]@{
        SchemaVersion=1; Day=$Day; Mode=$Mode; Port=$Port; StartedUtc=$StartedUtc; CompletedUtc=[DateTime]::UtcNow.ToString('o')
        Assertions=$Assertions
        Processes=@($OwnedProcesses | ForEach-Object { [ordered]@{ Name=$_.Name; ProcessId=$_.Process.Id; Arguments=$_.Arguments; Status=$_.Status; ExitCode=$_.ExitCode } })
        Artifacts=[ordered]@{ ServerLog=$ServerLog; Client1Log=$Client1Log; Client2Log=$Client2Log; Report=$ReportPath }
        Passed=$Passed; Failure=$Failure
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

if (-not $Passed) { Write-Error "[$DayLabel`NetworkSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[$DayLabel`NetworkSmoke][$Mode] PASS"
exit 0
