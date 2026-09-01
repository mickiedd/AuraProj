[CmdletBinding()]
param(
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [ValidateRange(1, 65535)]
    [int]$Port = 17826,
    [ValidateRange(5, 300)]
    [int]$StartupTimeoutSeconds = 60,
    [ValidateRange(10, 300)]
    [int]$AssertionTimeoutSeconds = 90,
    [ValidateRange(0, 1000)]
    [int]$NetworkLagMs = 0,
    [ValidateRange(0, 500)]
    [int]$NetworkLagVarianceMs = 0,
    [ValidateSet('Full', 'Cancel', 'NearClose')]
    [string]$Scenario = 'Full',
    [ValidateRange(1, 120)]
    [int]$TeardownTimeoutSeconds = 20
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'Aura.uproject'
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$ServerLog = Join-Path $LogDirectory 'CrunchCombo-Listen-Server.log'
$ClientLog = Join-Path $LogDirectory 'CrunchCombo-Listen-Client.log'
$ReportPath = Join-Path $ReportDirectory 'CrunchCombo-Listen.json'
$RunId = [Guid]::NewGuid().ToString('N')
$FixtureRoot = Join-Path $ProjectRoot "Saved\CrunchComboListen-$RunId"

if ([string]::IsNullOrWhiteSpace($EngineRoot) -or -not (Test-Path -LiteralPath $EditorExe)) { throw 'UE_ENGINE_ROOT is missing or UnrealEditor-Cmd.exe was not found.' }
if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Aura.uproject was not found at '$ProjectFile'." }

New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $FixtureRoot -Force | Out-Null
foreach ($Artifact in @($ServerLog, $ClientLog, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$OwnedProcesses = @()
$Assertions = [ordered]@{}
$Assertions.Scenario = $Scenario
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
    $ClientUserDir = Join-Path $FixtureRoot 'Client'
    foreach ($Directory in @($ServerUserDir, $ClientUserDir)) { New-Item -ItemType Directory -Path $Directory -Force | Out-Null }

    $Server = Start-OwnedProcess 'Server' @(
        $ProjectFile, '/Game/Maps/StartupMap?listen', '-server', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        "-port=$Port", '-CrunchComboNetworkProbe', "-CrunchComboNetworkProbeScenario=$Scenario", '-AuraPersistenceProvider=NULL', '-SaveToUserDir', "-UserDir=$ServerUserDir",
        "-PktLag=$NetworkLagMs", "-PktLagVariance=$NetworkLagVarianceMs", "-abslog=$ServerLog"
    )
    if (-not (Wait-ForPattern $ServerLog 'GameNetDriver.*Listening|Browse:.*StartupMap' $StartupTimeoutSeconds $Server)) {
        throw 'Listen server did not reach its startup gate.'
    }
    $Assertions.ServerStarted = $true

    $Client = Start-OwnedProcess 'Client' @(
        $ProjectFile, "127.0.0.1:${Port}?PlayerName=CrunchProbe?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        '-CrunchComboNetworkProbe', "-CrunchComboNetworkProbeScenario=$Scenario", '-SaveToUserDir', "-UserDir=$ClientUserDir",
        "-PktLag=$NetworkLagMs", "-PktLagVariance=$NetworkLagVarianceMs", "-abslog=$ClientLog"
    )
    if (-not (Wait-ForPattern $ServerLog '\[CrunchComboNetworkProbe\]\[Server\] RemoteActivationObserved=1' $AssertionTimeoutSeconds $Server)) {
        throw 'Server did not observe a remote combo activation.'
    }
    $Assertions.RemoteActivationObserved = $true
    if ($Scenario -eq 'Full') {
        if (-not (Wait-ForPattern $ServerLog '\[CrunchComboNetworkProbe\]\[Server\] PASS Open=4 Damage=4 Close=4 Accepted=4 Mask=0xF AuthorityDamage=1 Cleanup=1' $AssertionTimeoutSeconds $Server)) {
            throw 'Server did not observe the exact four-section combo matrix.'
        }
        $Assertions.ServerMatrix = $true
        if (-not (Wait-ForPattern $ClientLog '\[CrunchComboNetworkProbe\]\[Client\] COMPLETE Open=4 Damage=0 Close=4 Accepted=0 Mask=0x0 Presses=3 Cleanup=1' $AssertionTimeoutSeconds $Client)) {
            throw 'Remote client did not complete the four-section presentation/input matrix.'
        }
        $Assertions.ClientPresentationInputMatrix = $true
    }
    elseif ($Scenario -eq 'Cancel') {
        if (-not (Wait-ForPattern $ServerLog '\[CrunchComboNetworkProbe\]\[Server\] CANCEL_OBSERVED .*Damage=0 .*Accepted=0 .*Mask=0x0 .*Cleanup=1' $AssertionTimeoutSeconds $Server)) {
            throw 'Server did not observe cancellation with zero damage before reactivation.'
        }
        $Assertions.ServerCancelObserved = $true
        if (-not (Wait-ForPattern $ServerLog '\[CrunchComboNetworkProbe\]\[Server\] CANCEL_PASS Open=4 Damage=4 Close=4 Accepted=4 Mask=0xF AuthorityDamage=1 Cleanup=1' $AssertionTimeoutSeconds $Server)) {
            throw 'Server did not complete the full combo after network cancellation.'
        }
        $Assertions.ServerCancelReactivation = $true
        if (-not (Wait-ForPattern $ClientLog '\[CrunchComboNetworkProbe\]\[Client\] CANCEL_OBSERVED Inactive=1 .*Damage=0 .*Accepted=0 .*Mask=0x0 .*Cleanup=1' $AssertionTimeoutSeconds $Client)) {
            throw 'Client did not observe cancellation with zero damage.'
        }
        $Assertions.ClientCancelObserved = $true
        if (-not (Wait-ForPattern $ClientLog '\[CrunchComboNetworkProbe\]\[Client\] CANCEL_COMPLETE Open=4 Damage=0 Close=4 Accepted=0 Mask=0x0 Presses=3 CancelObserved=1 Cleanup=1' $AssertionTimeoutSeconds $Client)) {
            throw 'Client did not reactivate and complete the full presentation/input matrix.'
        }
        $Assertions.ClientCancelReactivation = $true
    }
    else {
        if (-not (Wait-ForPattern $ServerLog '\[CrunchComboNetworkProbe\]\[Server\] NEARCLOSE_(ACCEPTED|REJECTED) ' $AssertionTimeoutSeconds $Server)) {
            throw 'Server did not produce a near-close acceptance/rejection outcome.'
        }
        $ServerAccepted = Test-Pattern $ServerLog '\[CrunchComboNetworkProbe\]\[Server\] NEARCLOSE_ACCEPTED Open=4 Damage=4 Close=4 Accepted=4 Mask=0xF .*Cleanup=1'
        $ServerRejected = Test-Pattern $ServerLog '\[CrunchComboNetworkProbe\]\[Server\] NEARCLOSE_REJECTED Inactive=1 Open=[0-9]+ Damage=[0-9]+ Close=[0-9]+ Accepted=[0-9]+ Mask=0x[0-9A-F]+ .*Cleanup=1'
        if (-not $ServerAccepted -and -not $ServerRejected) { throw 'Server near-close outcome was not one of the supported terminal forms.' }
        $Assertions.ServerNearCloseOutcome = if ($ServerAccepted) { 'Accepted' } else { 'Rejected' }
        if ($ServerAccepted) {
            if (-not (Wait-ForPattern $ClientLog '\[CrunchComboNetworkProbe\]\[Client\] NEARCLOSE_ACCEPTED Inactive=1 Open=4 Damage=0 Close=4 Accepted=0 Mask=0x0 Presses=[0-9]+ Cleanup=1' $AssertionTimeoutSeconds $Client)) {
                throw 'Client did not converge on the accepted near-close outcome.'
            }
            $Assertions.ClientNearCloseOutcome = 'Accepted'
        }
        else {
            $ServerNearCloseLine = Select-String -LiteralPath $ServerLog -Pattern '\[CrunchComboNetworkProbe\]\[Server\] NEARCLOSE_REJECTED' | Select-Object -Last 1
            $ServerNearCloseMatch = [regex]::Match($ServerNearCloseLine.Line, 'Open=(\d+)')
            if (-not $ServerNearCloseMatch.Success) { throw 'Could not extract the server near-close section.' }
            $ExpectedOpen = $ServerNearCloseMatch.Groups[1].Value
            $ClientConvergedPattern = '\[CrunchComboNetworkProbe\]\[Client\] NEARCLOSE_CONVERGED Inactive=1 Open=' + $ExpectedOpen + ' Damage=0 Close=[0-9]+ Accepted=0 Mask=0x0 Presses=[0-9]+ Cleanup=1'
            if (-not (Wait-ForPattern $ClientLog $ClientConvergedPattern $AssertionTimeoutSeconds $Client)) {
                throw "Client did not converge to the server near-close section Open=$ExpectedOpen."
            }
            $Assertions.ClientNearCloseOutcome = "ConvergedOpen=$ExpectedOpen"
        }
    }
    $Assertions.NoCrashOrFatal = -not (@($ServerLog, $ClientLog) | Where-Object { Test-Pattern $_ 'Fatal error:|Assertion failed:|Unhandled Exception' })
    if (-not $Assertions.NoCrashOrFatal) { throw 'A Crunch combo network process reported a fatal/crash signature.' }
    $Passed = $true
}
catch {
    $Failure = $_.Exception.Message
}
finally {
    foreach ($Owned in @($OwnedProcesses | Sort-Object Name -Descending)) { Stop-OwnedProcess $Owned }
    $MissingArtifacts = @(@($ServerLog, $ClientLog) | Where-Object { -not (Test-Path -LiteralPath $_) })
    if ($MissingArtifacts.Count -gt 0) {
        $Passed = $false
        $Text = "Missing artifact(s): $($MissingArtifacts -join ', ')"
        $Failure = if ($Failure) { "$Failure $Text" } else { $Text }
    }
    $FixtureRemoved = $false
    $ResolvedFixtureRoot = [IO.Path]::GetFullPath($FixtureRoot)
    $ResolvedSavedRoot = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved'))
    if ($ResolvedFixtureRoot.StartsWith($ResolvedSavedRoot, [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $ResolvedFixtureRoot)) {
        Remove-Item -LiteralPath $ResolvedFixtureRoot -Recurse -Force
        $FixtureRemoved = -not (Test-Path -LiteralPath $ResolvedFixtureRoot)
    }
    if (@($OwnedProcesses | Where-Object { $_.Status -eq 'TeardownTimeout' }).Count -gt 0) {
        $Passed = $false
        $Failure = if ($Failure) { "$Failure Owned process teardown timed out." } else { 'Owned process teardown timed out.' }
    }
    $Report = [ordered]@{
        SchemaVersion=1; Mode='Listen'; Port=$Port; NetworkLagMs=$NetworkLagMs; NetworkLagVarianceMs=$NetworkLagVarianceMs; StartedUtc=$StartedUtc; CompletedUtc=[DateTime]::UtcNow.ToString('o')
        Assertions=$Assertions
        Processes=@($OwnedProcesses | ForEach-Object { [ordered]@{ Name=$_.Name; ProcessId=$_.Process.Id; Arguments=$_.Arguments; Status=$_.Status; ExitCode=$_.ExitCode } })
        Artifacts=[ordered]@{ ServerLog=$ServerLog; ClientLog=$ClientLog; Report=$ReportPath }
        IsolatedFixtureRemoved=$FixtureRemoved; Passed=$Passed; Failure=$Failure
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

if (-not $Passed) { Write-Error "[CrunchComboNetworkSmoke][Listen] FAIL: $Failure"; exit 1 }
Write-Host '[CrunchComboNetworkSmoke][Listen] PASS'
exit 0
