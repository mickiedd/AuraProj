[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$ArchivePath,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [ValidateRange(1, 65535)] [int]$Port = 17907,
    [ValidateRange(30, 300)] [int]$TimeoutSeconds = 120
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$ServerLog = Join-Path $LogDirectory 'Day07-Packaged-Server.log'
$Client1Log = Join-Path $LogDirectory 'Day07-Packaged-Client1.log'
$Client2Log = Join-Path $LogDirectory 'Day07-Packaged-Client2.log'
$ReportPath = Join-Path $ReportDirectory 'Day07-Packaged.json'
$RunId = [Guid]::NewGuid().ToString('N')
$FixtureRoot = Join-Path $ProjectRoot "Saved\RoleBattleDay7-Packaged-$RunId"
$AbilityDefinitionRoot = Join-Path $ProjectRoot 'Content\AbilityDefinitions'
$RequiredDefinitions = @(Get-ChildItem -LiteralPath $AbilityDefinitionRoot -File -Filter '*.xml' | Sort-Object Name | Select-Object -ExpandProperty Name)
if ($RequiredDefinitions.Count -eq 0) { throw "No shipped ability-definition XML files were found under '$AbilityDefinitionRoot'." }
$WorldPersistenceId = "RoleBattleDay7Packaged-$RunId"

function Resolve-FullPath([string]$Path) { return [IO.Path]::GetFullPath((Resolve-Path -LiteralPath $Path).Path) }
function Test-Pattern([string]$Path, [string]$Pattern) { return (Test-Path -LiteralPath $Path) -and [bool](Select-String -LiteralPath $Path -Pattern $Pattern -Quiet) }
function Get-NetworkVersion([string]$Path) {
    if (-not (Test-Path -LiteralPath $Path)) { return $null }
    foreach ($Line in @(Get-Content -LiteralPath $Path -ErrorAction SilentlyContinue)) {
        if ($Line -match 'LogNetVersion: .*Checksum: (\d+)') { return $Matches[1] }
    }
    return $null
}
function Get-State([System.Diagnostics.Process]$Process) {
    if ($null -eq $Process) { return 'NotStarted' }
    try { $Process.Refresh(); if ($Process.HasExited) { return 'Exited' }; return 'Running' } catch { return 'Unavailable' }
}
function Get-DescendantProcessIds([int]$RootProcessId) {
    $Ids = @($RootProcessId)
    $Changed = $true
    while ($Changed) {
        $Changed = $false
        foreach ($Process in @(Get-CimInstance Win32_Process)) {
            $ProcessId = [int]$Process.ProcessId
            if (($Ids -contains [int]$Process.ParentProcessId) -and -not ($Ids -contains $ProcessId)) {
                $Ids += $ProcessId
                $Changed = $true
            }
        }
    }
    return @($Ids)
}
function Start-Owned([string]$Name, [string]$Executable, [string]$WorkingDirectory, [string[]]$Arguments) {
    $Process = Start-Process -FilePath $Executable -WorkingDirectory $WorkingDirectory -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    return [pscustomobject]@{ Name=$Name; Process=$Process; ChildProcessIds=@(); Executable=$Executable; WorkingDirectory=$WorkingDirectory; Arguments=@($Arguments); Status='Running'; ExitCode=$null }
}
function Stop-Owned([pscustomobject]$Owned) {
    if ($null -eq $Owned -or $null -eq $Owned.Process) { return }
    $Owned.ChildProcessIds = @(Get-DescendantProcessIds $Owned.Process.Id | Where-Object { $_ -ne $Owned.Process.Id })
    $AllProcessIds = @($Owned.Process.Id) + @($Owned.ChildProcessIds)
    foreach ($ProcessId in @($AllProcessIds | Sort-Object -Descending)) { Stop-Process -Id $ProcessId -Force -ErrorAction SilentlyContinue }
    $Deadline = [DateTime]::UtcNow.AddSeconds(10)
    while ((@($AllProcessIds | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue }).Count -gt 0) -and [DateTime]::UtcNow -lt $Deadline) { Start-Sleep -Milliseconds 100 }
    $Owned.Status = if (@($AllProcessIds | Where-Object { Get-Process -Id $_ -ErrorAction SilentlyContinue }).Count -gt 0) { 'TeardownTimeout' } else { 'Stopped' }
    try { $Owned.ExitCode = $Owned.Process.ExitCode } catch { $Owned.ExitCode = $null }
}
function Wait-ForPattern([string]$Path, [string]$Pattern, [int]$Seconds, [pscustomobject]$RequiredProcess) {
    $Deadline = [DateTime]::UtcNow.AddSeconds($Seconds)
    do {
        if (Test-Pattern $Path $Pattern) { return $true }
        if ($RequiredProcess -and (Get-State $RequiredProcess.Process) -ne 'Running') { return $false }
        Start-Sleep -Milliseconds 250
    } while ([DateTime]::UtcNow -lt $Deadline)
    return $false
}

if (-not (Test-Path -LiteralPath $ArchivePath)) { throw "Packaged archive path '$ArchivePath' does not exist." }
$ArchiveRoot = Resolve-FullPath $ArchivePath
New-Item -ItemType Directory -Path $LogDirectory, $ReportDirectory, $FixtureRoot -Force | Out-Null
foreach ($Artifact in @($ServerLog, $Client1Log, $Client2Log, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$ClientExe = Get-ChildItem -LiteralPath $ArchiveRoot -Recurse -File -Filter 'Aura.exe' | Where-Object { $_.FullName -notmatch '\\Binaries\\' } | Select-Object -First 1
$ServerExe = Get-ChildItem -LiteralPath $ArchiveRoot -Recurse -File -Filter 'AuraServer.exe' | Where-Object { $_.FullName -notmatch '\\Binaries\\' } | Select-Object -First 1
if (-not $ClientExe) { throw "Packaged client launcher Aura.exe was not found under '$ArchiveRoot'." }
if (-not $ServerExe) { throw "Packaged server launcher AuraServer.exe was not found under '$ArchiveRoot'." }
if (-not (Test-Path -LiteralPath (Join-Path $ClientExe.Directory.FullName 'Aura\Content\Paks'))) { throw 'Packaged client content paks are missing.' }
if (-not (Test-Path -LiteralPath (Join-Path $ServerExe.Directory.FullName 'Aura\Content\Paks'))) { throw 'Packaged server content paks are missing.' }

$ClientManifestText = (Get-ChildItem -LiteralPath $ClientExe.Directory.FullName -Recurse -File -Filter 'Manifest_*' | Get-Content -Raw -ErrorAction SilentlyContinue) -join "`n"
$ServerManifestText = (Get-ChildItem -LiteralPath $ServerExe.Directory.FullName -Recurse -File -Filter 'Manifest_*' | Get-Content -Raw -ErrorAction SilentlyContinue) -join "`n"
foreach ($Definition in $RequiredDefinitions) {
    $ClientLooseDefinition = Get-ChildItem -LiteralPath $ClientExe.Directory.FullName -Recurse -File -Filter $Definition | Where-Object { $_.FullName -match '\\(AbilityDefinitions|Content\\AbilityDefinitions)\\' } | Select-Object -First 1
    $ServerLooseDefinition = Get-ChildItem -LiteralPath $ServerExe.Directory.FullName -Recurse -File -Filter $Definition | Where-Object { $_.FullName -match '\\(AbilityDefinitions|Content\\AbilityDefinitions)\\' } | Select-Object -First 1
    $ClientManifestDefinition = $ClientManifestText -match "(?im)(^|[/\\])(?:Aura/)?Content[/\\]AbilityDefinitions[/\\]$([regex]::Escape($Definition))"
    $ServerManifestDefinition = $ServerManifestText -match "(?im)(^|[/\\])(?:Aura/)?Content[/\\]AbilityDefinitions[/\\]$([regex]::Escape($Definition))"
    if ((-not $ClientLooseDefinition -and -not $ClientManifestDefinition) -or (-not $ServerLooseDefinition -and -not $ServerManifestDefinition)) {
        throw "Packaged definition '$Definition' is absent from the client or server staging manifest."
    }
}

$OwnedProcesses = @()
$Assertions = [ordered]@{
    PackagedClientExecutable = $ClientExe.FullName
    PackagedServerExecutable = $ServerExe.FullName
    AbilityDefinitionFiles = @($RequiredDefinitions)
    PackagedDefinitionManifest = $true
}
$Passed = $false
$Failure = $null
$StartedUtc = [DateTime]::UtcNow.ToString('o')

try {
    $ServerUserDir = Join-Path $FixtureRoot 'Server'
    $Client1UserDir = Join-Path $FixtureRoot 'Client1'
    $Client2UserDir = Join-Path $FixtureRoot 'Client2'
    New-Item -ItemType Directory -Path $ServerUserDir, $Client1UserDir, $Client2UserDir -Force | Out-Null

    $Server = Start-Owned 'Server' $ServerExe.FullName $ServerExe.Directory.FullName @(
        '/Game/Maps/StartupMap', '-server', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        "-port=$Port", '-RoleBattleDay6NetworkProbe', "-WorldPersistenceId=$WorldPersistenceId", '-AuraPersistenceProvider=NULL', '-SaveToUserDir', "-UserDir=$ServerUserDir", "-abslog=$ServerLog"
    )
    $OwnedProcesses += $Server
    if (-not (Wait-ForPattern $ServerLog 'GameNetDriver.*Listening|Browse:.*StartupMap' 45 $Server)) { throw 'Packaged server did not reach its listening startup gate.' }
    $Assertions.ServerStarted = $true

    $Client1 = Start-Owned 'Client1' $ClientExe.FullName $ClientExe.Directory.FullName @(
        "127.0.0.1:${Port}?PlayerName=Day7PackagedAura?Role=Aura", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        '-RoleBattleDay6NetworkProbe', '-SaveToUserDir', "-UserDir=$Client1UserDir", "-abslog=$Client1Log"
    )
    $OwnedProcesses += $Client1
    $Client2 = Start-Owned 'Client2' $ClientExe.FullName $ClientExe.Directory.FullName @(
        "127.0.0.1:${Port}?PlayerName=Day7PackagedCrunch?Role=Crunch", '-game', '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
        '-RoleBattleDay6NetworkProbe', '-SaveToUserDir', "-UserDir=$Client2UserDir", "-abslog=$Client2Log"
    )
    $OwnedProcesses += $Client2

    if (-not (Wait-ForPattern $ServerLog 'LogNetVersion: .*Checksum: \d+' 45 $Server)) { throw 'Packaged server did not publish a network checksum.' }
    if (-not (Wait-ForPattern $Client1Log 'LogNetVersion: .*Checksum: \d+' 45 $Client1)) { throw 'Packaged Aura client did not publish a network checksum.' }
    if (-not (Wait-ForPattern $Client2Log 'LogNetVersion: .*Checksum: \d+' 45 $Client2)) { throw 'Packaged Crunch client did not publish a network checksum.' }
    $ServerNetworkVersion = Get-NetworkVersion $ServerLog
    $Client1NetworkVersion = Get-NetworkVersion $Client1Log
    $Client2NetworkVersion = Get-NetworkVersion $Client2Log
    if ([string]::IsNullOrWhiteSpace($ServerNetworkVersion) -or [string]::IsNullOrWhiteSpace($Client1NetworkVersion) -or [string]::IsNullOrWhiteSpace($Client2NetworkVersion)) {
        throw 'Packaged client/server network checksum could not be parsed.'
    }
    if ($ServerNetworkVersion -ne $Client1NetworkVersion -or $ServerNetworkVersion -ne $Client2NetworkVersion) {
        throw "Packaged client/server network checksum mismatch: server=$ServerNetworkVersion client1=$Client1NetworkVersion client2=$Client2NetworkVersion. Rebuild and distribute one synchronized package."
    }
    $Assertions.NetworkVersionMatch = $true
    $Assertions.NetworkVersionChecksums = [ordered]@{ Server=$ServerNetworkVersion; Client1=$Client1NetworkVersion; Client2=$Client2NetworkVersion }

    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] Role=Aura Combat=Combat\.Magic.*ExistingSaveReconciliation=1.*LiveSwitchRejected=1' $TimeoutSeconds $Server)) { throw 'Packaged Aura authoritative role audit failed.' }
    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] Role=Crunch.*LiveSwitchRejected=1' $TimeoutSeconds $Server)) { throw 'Packaged Crunch authoritative role audit failed.' }
    if (-not (Wait-ForPattern $Client1Log '\[Day6NetworkProbe\]\[Client\] Role=Aura Combat=Combat\.Magic Presentation=1 ClientRoleMutationRejected=1 AuthorityGrantMutation=0' $TimeoutSeconds $Client1)) { throw 'Packaged Aura client audit failed.' }
    if (-not (Wait-ForPattern $Client2Log '\[Day6NetworkProbe\]\[Client\] Role=Crunch.*ClientRoleMutationRejected=1 AuthorityGrantMutation=0' $TimeoutSeconds $Client2)) { throw 'Packaged Crunch client audit failed.' }
    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] PASS Role=Aura Respawns=2 Profiles=1 Idempotent=1' $TimeoutSeconds $Server)) { throw 'Packaged Aura respawn audit failed.' }
    if (-not (Wait-ForPattern $ServerLog '\[Day6NetworkProbe\]\[Server\] PASS Role=Crunch Respawns=2 Profiles=1 Idempotent=1' $TimeoutSeconds $Server)) { throw 'Packaged Crunch respawn audit failed.' }
    $Assertions.AuthoritativeProfiles = $true
    $Assertions.ClientPresentationAndMutationRejection = $true
    $Assertions.TwoRespawnsPerRole = $true

    $ProbeLogs = @($ServerLog, $Client1Log, $Client2Log)
    if (@($ProbeLogs | Where-Object { Test-Pattern $_ 'Fatal error:|Assertion failed:|Unhandled Exception|Ensure condition failed' }).Count -gt 0) { throw 'A packaged process reported a fatal/crash signature.' }
    if (@($OwnedProcesses | Where-Object { (Get-State $_.Process) -ne 'Running' }).Count -gt 0) { throw 'A packaged process exited before assertions completed.' }
    $Assertions.NoCrashOrUnexpectedExit = $true
    $Passed = $true
}
catch { $Failure = $_.Exception.Message }
finally {
    foreach ($Owned in @($OwnedProcesses | Sort-Object Name -Descending)) { Stop-Owned $Owned }
    if (@($OwnedProcesses | Where-Object { $_.Status -eq 'TeardownTimeout' }).Count -gt 0) { $Passed = $false; $Failure = if ($Failure) { "$Failure Owned process teardown timed out." } else { 'Owned process teardown timed out.' } }
    $Required = @($ServerLog, $Client1Log, $Client2Log)
    $Missing = @($Required | Where-Object { -not (Test-Path -LiteralPath $_) })
    if ($Missing.Count -gt 0) { $Passed = $false; $Failure = if ($Failure) { "$Failure Missing artifact(s): $($Missing -join ', ')" } else { "Missing artifact(s): $($Missing -join ', ')" } }
    $ResolvedFixture = [IO.Path]::GetFullPath($FixtureRoot)
    $ResolvedSaved = [IO.Path]::GetFullPath((Join-Path $ProjectRoot 'Saved'))
    if ($ResolvedFixture.StartsWith($ResolvedSaved, [StringComparison]::OrdinalIgnoreCase) -and (Test-Path -LiteralPath $ResolvedFixture)) { Remove-Item -LiteralPath $ResolvedFixture -Recurse -Force }
    $Report = [ordered]@{
        SchemaVersion=1; Mode='Packaged'; ArchivePath=$ArchiveRoot; StartedUtc=$StartedUtc; CompletedUtc=[DateTime]::UtcNow.ToString('o')
        Assertions=$Assertions; Processes=@($OwnedProcesses | ForEach-Object { [ordered]@{ Name=$_.Name; ProcessId=$_.Process.Id; ChildProcessIds=$_.ChildProcessIds; Executable=$_.Executable; Status=$_.Status; ExitCode=$_.ExitCode } })
        Artifacts=[ordered]@{ ServerLog=$ServerLog; Client1Log=$Client1Log; Client2Log=$Client2Log; Report=$ReportPath }
        Passed=$Passed; Failure=$Failure
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

if (-not $Passed) { Write-Error "[Day07PackagedTopology] FAIL: $Failure"; exit 1 }
Write-Host '[Day07PackagedTopology] PASS'
exit 0
