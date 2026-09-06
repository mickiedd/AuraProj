[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('Listen', 'Dedicated')]
    [string]$Mode,
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [ValidateRange(1, 65535)]
    [int]$ListenPort = 17781,
    [ValidateRange(5, 300)]
    [int]$StartupTimeoutSeconds = 40,
    [ValidateRange(5, 300)]
    [int]$AssertionTimeoutSeconds = 50,
    [ValidateRange(1, 120)]
    [int]$TeardownTimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'Aura.uproject'
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$RoleConfigPath = Join-Path $ProjectRoot 'Content\Config\RoleConfig.json'
$SentinelPath = Join-Path $ProjectRoot 'Content\Config\.RoleConfig.reload'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$ServerLog = Join-Path $LogDirectory "Day05-$Mode-Server.log"
$Client1Log = Join-Path $LogDirectory "Day05-$Mode-Client1.log"
$Client2Log = Join-Path $LogDirectory "Day05-$Mode-Client2.log"
$InvalidClientLog = Join-Path $LogDirectory "Day05-$Mode-InvalidClient.log"
$InvalidStartupLog = Join-Path $LogDirectory "Day05-$Mode-InvalidStartup.log"
$ReportPath = Join-Path $ReportDirectory "Day05-$Mode.json"
$RunId = [Guid]::NewGuid().ToString('N')
$InvalidWorldPersistenceId = "RoleBattleDay5-Invalid-$Mode-$RunId"
$ValidWorldPersistenceId = "RoleBattleDay5-Valid-$Mode-$RunId"

if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path -LiteralPath $EditorExe)) { throw 'UE_ENGINE_ROOT is missing or UnrealEditor-Cmd.exe was not found.' }
if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Aura.uproject was not found at '$ProjectFile'." }
if (-not (Test-Path -LiteralPath $RoleConfigPath)) { throw "RoleConfig.json was not found at '$RoleConfigPath'." }

New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $InvalidClientLog, $InvalidStartupLog, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$OriginalConfig = [IO.File]::ReadAllText($RoleConfigPath)
$SentinelExisted = Test-Path -LiteralPath $SentinelPath
$OriginalSentinel = if ($SentinelExisted) { [IO.File]::ReadAllText($SentinelPath) } else { $null }
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
    param([string]$Path, [string]$Pattern, [int]$TimeoutSeconds, [pscustomobject]$Owned)
    $Deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if (Test-Pattern $Path $Pattern) { return $true }
        if ($Owned -and (Get-ProcessState $Owned.Process) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}

function Write-ReloadSentinel {
    [IO.File]::WriteAllText($SentinelPath, [DateTime]::UtcNow.ToString('o'))
}

try {
    # Prove a never-good startup stays unavailable. This process is owned and bounded.
    [IO.File]::WriteAllText($RoleConfigPath, '{"roleDefinitionVersion":2,"defaultRole":"Missing","roles":[]}')
    $InvalidMap = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?listen' } else { '/Game/Maps/StartupMap' }
    $InvalidServer = Start-OwnedProcess 'InvalidStartupServer' @($ProjectFile, $InvalidMap, '-server', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-port=$ListenPort", "-WorldPersistenceId=$InvalidWorldPersistenceId", '-AuraPersistenceProvider=NULL', '-RoleBattleDay5ConfigProbe', "-abslog=$InvalidStartupLog")
    if (-not (Wait-ForPattern $InvalidStartupLog '\[Day5ConfigProbe\]\[Server\] InvalidStartupRejected=1 RoleServiceUnavailable=1' $StartupTimeoutSeconds $InvalidServer)) {
        throw 'Invalid startup was not rejected before the startup timeout.'
    }
    $Assertions.InvalidStartupRejected = $true
    Stop-OwnedProcess $InvalidServer

    [IO.File]::WriteAllText($RoleConfigPath, $OriginalConfig)
    Start-Sleep -Milliseconds 1100

    $ServerMap = if ($Mode -eq 'Listen') { '/Game/Maps/StartupMap?listen' } else { '/Game/Maps/StartupMap' }
    $Server = Start-OwnedProcess 'Server' @($ProjectFile, $ServerMap, '-server', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-port=$ListenPort", "-WorldPersistenceId=$ValidWorldPersistenceId", '-AuraPersistenceProvider=NULL', '-RoleBattleDay5ConfigProbe', "-abslog=$ServerLog")
    if (-not (Wait-ForPattern $ServerLog '\[Day5ConfigProbe\]\[Server\] ValidStartup=1.*SavedDefaultValidation=1' $StartupTimeoutSeconds $Server)) {
        throw 'Valid startup publication did not complete before timeout.'
    }
    $Assertions.ValidStartupPublished = $true

    # A malformed sentinel reload must retain the object already serving logins.
    [IO.File]::WriteAllText($RoleConfigPath, '{"roleDefinitionVersion":2,"defaultRole":"Broken","roles":[]}')
    Write-ReloadSentinel
    if (-not (Wait-ForPattern $ServerLog 'Reload rejected; retaining last known-good registry' $AssertionTimeoutSeconds $Server)) {
        throw 'Bad sentinel reload did not report last-good retention.'
    }
    $Assertions.BadReloadRetainedLastGood = $true

    # Restore a good candidate and prove atomic publication resumes.
    [IO.File]::WriteAllText($RoleConfigPath, $OriginalConfig)
    Start-Sleep -Milliseconds 1100
    Write-ReloadSentinel
    if (-not (Wait-ForPattern $ServerLog '\[RoleConfig\] Reloaded: 3 role\(s\)' $AssertionTimeoutSeconds $Server)) {
        throw 'Good sentinel reload did not publish.'
    }
    $Assertions.GoodReloadPublished = $true

    $InvalidClient = Start-OwnedProcess 'InvalidClient' @($ProjectFile, "127.0.0.1:${ListenPort}?PlayerName=Day5Invalid?Role=NotARealRole", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$InvalidClientLog")
    if (-not (Wait-ForPattern $ServerLog '\[RoleLogin\]\[PreLogin\] Rejected.*role=NotARealRole' $AssertionTimeoutSeconds $Server)) {
        throw 'Invalid connection role was not rejected by PreLogin.'
    }
    $Assertions.InvalidRoleRejected = $true
    Stop-OwnedProcess $InvalidClient

    $Client1 = Start-OwnedProcess 'Client1' @($ProjectFile, "127.0.0.1:${ListenPort}?PlayerName=Day5Aura?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$Client1Log")
        $Client2 = Start-OwnedProcess 'Client2' @($ProjectFile, "127.0.0.1:${ListenPort}?PlayerName=Day5Crunch?Role=Crunch", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash', "-abslog=$Client2Log")
    $ConnectionDeadline = [DateTime]::UtcNow.AddSeconds($AssertionTimeoutSeconds)
    do {
        if ((Get-ProcessState $Server.Process) -ne 'Running') { throw 'Server exited during connection assertions.' }
        $AuraPending = Test-Pattern $ServerLog '\[RoleLogin\]\[InitNewPlayer\] PendingAcceptedRoleId=Aura '
            $CrunchPending = Test-Pattern $ServerLog '\[RoleLogin\]\[InitNewPlayer\] PendingAcceptedRoleId=Crunch '
        if ($AuraPending -and $CrunchPending) { break }
        Start-Sleep -Milliseconds 500
    } while ([DateTime]::UtcNow -lt $ConnectionDeadline)
    $Assertions.AuraConnectionScopedRole = $AuraPending
    $Assertions.CrunchConnectionScopedRole = $CrunchPending
    if (-not $AuraPending -or -not $CrunchPending) { throw 'Two simultaneous connections did not retain distinct accepted role IDs.' }

    $PublishedLine = Select-String -LiteralPath $ServerLog -Pattern '\[RoleConfig\]\[InitGame\] Published' | Select-Object -First 1
    $FirstPreLogin = Select-String -LiteralPath $ServerLog -Pattern '\[RoleLogin\]\[PreLogin\]' | Select-Object -First 1
    $Assertions.InitGameBeforePreLogin = $null -ne $PublishedLine -and $null -ne $FirstPreLogin -and $PublishedLine.LineNumber -lt $FirstPreLogin.LineNumber
    $Assertions.SavedAndDefaultRoleValidation = Test-Pattern $ServerLog 'SavedDefaultValidation=1'
    if (-not $Assertions.InitGameBeforePreLogin) { throw 'InitGame publication did not precede PreLogin.' }
    if (-not $Assertions.SavedAndDefaultRoleValidation) { throw 'Saved/default role validation probe failed.' }
    $Passed = $true
}
catch {
    $Failure = $_.Exception.Message
}
finally {
    # Restore source config and pre-existing sentinel state even on assertion/process failure.
    [IO.File]::WriteAllText($RoleConfigPath, $OriginalConfig)
    if ($SentinelExisted) { [IO.File]::WriteAllText($SentinelPath, $OriginalSentinel) }
    elseif (Test-Path -LiteralPath $SentinelPath) { Remove-Item -LiteralPath $SentinelPath -Force }

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
    $Report = [ordered]@{
        SchemaVersion=1; Mode=$Mode; ServerRuntime='UnrealEditor-Cmd server'; Port=$ListenPort; WorldPersistenceIds=@($InvalidWorldPersistenceId, $ValidWorldPersistenceId)
        StartedUtc=$StartedUtc; CompletedUtc=[DateTime]::UtcNow.ToString('o'); Assertions=$Assertions
        Processes=@($OwnedProcesses | ForEach-Object { [ordered]@{ Name=$_.Name; ProcessId=$_.Process.Id; Arguments=$_.Arguments; Status=$_.Status; ExitCode=$_.ExitCode } })
        Artifacts=[ordered]@{ ServerLog=$ServerLog; Client1Log=$Client1Log; Client2Log=$Client2Log; InvalidClientLog=$InvalidClientLog; InvalidStartupLog=$InvalidStartupLog; Report=$ReportPath }
        ConfigRestored=([IO.File]::ReadAllText($RoleConfigPath) -ceq $OriginalConfig); SentinelRestored=((Test-Path -LiteralPath $SentinelPath) -eq $SentinelExisted)
        Passed=$Passed; Failure=$Failure
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

if (-not $Passed) { Write-Error "[Day5ConfigSmoke][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[Day5ConfigSmoke][$Mode] PASS"
exit 0
