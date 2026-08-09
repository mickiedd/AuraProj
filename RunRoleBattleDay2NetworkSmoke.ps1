[CmdletBinding()]
param(
    [ValidateSet('Both', 'Listen', 'Dedicated')]
    [string]$Mode = 'Both',
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [Alias('Port')]
    [ValidateRange(1, 65535)]
    [int]$ListenPort = 17777,
    [ValidateRange(1, 65535)]
    [int]$DedicatedPort = 17778,
    [ValidateRange(1, 300)]
    [int]$StartupTimeoutSeconds = 20,
    [Alias('TimeoutSeconds')]
    [ValidateRange(1, 600)]
    [int]$AssertionTimeoutSeconds = 35,
    [ValidateRange(1, 120)]
    [int]$TeardownTimeoutSeconds = 10
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectFile = Join-Path $projectRoot 'Aura.uproject'
$logDirectory = Join-Path $projectRoot 'Saved\Logs'
$reportDirectory = Join-Path $projectRoot 'Saved\Reports'

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    Write-Error 'UE_ENGINE_ROOT is not set. Set it to the Unreal Engine root.'
    exit 1
}

$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$cookedStartupMap = Join-Path $projectRoot 'Saved\Cooked\WindowsServer\Aura\Content\Maps\StartupMap.umap'
if (-not (Test-Path -LiteralPath $editorExe)) {
    Write-Error "UnrealEditor-Cmd.exe was not found. Set UE_ENGINE_ROOT to the Unreal Engine root."
    exit 1
}
if (-not (Test-Path -LiteralPath $projectFile)) {
    Write-Error "Aura.uproject was not found at '$projectFile'."
    exit 1
}

New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null

$selectedModes = if ($Mode -eq 'Both') { @('Listen', 'Dedicated') } else { @($Mode) }
$revision = [string]((& git -C $projectRoot rev-parse HEAD 2>$null))
if ([string]::IsNullOrWhiteSpace($revision)) {
    $revision = 'unknown'
}

function Test-LogPattern {
    param(
        [string]$Path,
        [string]$Pattern
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }

    return [bool](Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)
}

function Test-LogPatternCount {
    param(
        [string]$Path,
        [string]$Pattern,
        [int]$MinimumCount
    )

    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }

    $matches = @(Select-String -LiteralPath $Path -Pattern $Pattern)
    return $matches.Count -ge $MinimumCount
}

function Get-ManagedProcessStatus {
    param([System.Diagnostics.Process]$Process)

    if ($null -eq $Process) {
        return 'NotStarted'
    }

    try {
        $Process.Refresh()
        if ($Process.HasExited) {
            return 'Exited'
        }
        return 'Running'
    }
    catch {
        return 'Unavailable'
    }
}

function Start-ManagedProcess {
    param(
        [string]$Name,
        [string]$Executable,
        [string[]]$Arguments
    )

    $process = Start-Process -FilePath $Executable -ArgumentList $Arguments -PassThru -WindowStyle Hidden
    $record = [ordered]@{
        Name = $Name
        ProcessId = $process.Id
        Executable = $Executable
        Arguments = @($Arguments)
        Status = 'Running'
        ExitCode = $null
    }

    return [pscustomobject]@{
        Process = $process
        Record = $record
    }
}

function Stop-ManagedProcess {
    param(
        [pscustomobject]$ManagedProcess,
        [int]$TimeoutSeconds
    )

    if ($null -eq $ManagedProcess -or $null -eq $ManagedProcess.Process) {
        return
    }

    $process = $ManagedProcess.Process
    $status = Get-ManagedProcessStatus -Process $process
    $wasRunning = $status -eq 'Running'
    if ($status -eq 'Running') {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        $status = Get-ManagedProcessStatus -Process $process
        if ($status -eq 'Exited') {
            break
        }
        if ([DateTime]::UtcNow -ge $deadline) {
            break
        }
        Start-Sleep -Milliseconds 100
    } while ($true)

    if ($status -eq 'Exited') {
        $ManagedProcess.Record.Status = if ($wasRunning) { 'Stopped' } else { 'Exited' }
        try {
            $ManagedProcess.Record.ExitCode = $process.ExitCode
        }
        catch {
            $ManagedProcess.Record.ExitCode = $null
        }
    }
    else {
        $ManagedProcess.Record.Status = 'TeardownTimeout'
    }
}

function Get-NetworkAssertions {
    param(
        [string]$ServerLog,
        [string]$Client1Log,
        [string]$Client2Log
    )

    return [ordered]@{
        ServerAcceptedTwoClients = Test-LogPatternCount -Path $ServerLog -Pattern 'Join succeeded:' -MinimumCount 2
        ServerPlayerIdentity = Test-LogPattern -Path $ServerLog -Pattern '\[CombatIdentity\]\[Server\].*Faction=Faction\.Player.*Control=Control\.Player.*Profile=Combat\.Unassigned.*Death=Death\.PlayerRespawn.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        ServerEnemyIdentity = Test-LogPattern -Path $ServerLog -Pattern '\[CombatIdentity\]\[Server\].*Faction=Faction\.Enemy.*Control=Control\.EnemyAI.*Profile=Combat\.Unassigned.*Death=Death\.EnemyLoot.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        Client1PlayerIdentity = Test-LogPattern -Path $Client1Log -Pattern '\[CombatIdentity\]\[Client\].*Faction=Faction\.Player.*Control=Control\.Player.*Profile=Combat\.Unassigned.*Death=Death\.PlayerRespawn.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        Client1EnemyIdentity = Test-LogPattern -Path $Client1Log -Pattern '\[CombatIdentity\]\[Client\].*Faction=Faction\.Enemy.*Control=Control\.EnemyAI.*Profile=Combat\.Unassigned.*Death=Death\.EnemyLoot.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        Client2PlayerIdentity = Test-LogPattern -Path $Client2Log -Pattern '\[CombatIdentity\]\[Client\].*Faction=Faction\.Player.*Control=Control\.Player.*Profile=Combat\.Unassigned.*Death=Death\.PlayerRespawn.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        Client2EnemyIdentity = Test-LogPattern -Path $Client2Log -Pattern '\[CombatIdentity\]\[Client\].*Faction=Faction\.Enemy.*Control=Control\.EnemyAI.*Profile=Combat\.Unassigned.*Death=Death\.EnemyLoot.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        EnemyAcquiredPlayer = Test-LogPattern -Path $ServerLog -Pattern '\[EnemyAI\]\[FindNearestPlayer\].*Candidates=[1-9][0-9]*.*Closest=BP_AuraCharacter.*BB: Target=BP_AuraCharacter.*BBDist='
    }
}

function Get-DedicatedServerExecutable {
    $candidates = @(
        (Join-Path $projectRoot 'Binaries\Win64\AuraServer.exe'),
        (Join-Path $projectRoot 'Binaries\Win64\AuraServer-Win64-Development.exe'),
        (Join-Path $projectRoot 'Binaries\Win64\AuraServer-Win64-DebugGame.exe')
    )

    foreach ($candidate in $candidates) {
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }
    return $null
}

function Test-AllAssertions {
    param([System.Collections.IDictionary]$Assertions)

    foreach ($value in $Assertions.Values) {
        if (-not [bool]$value) {
            return $false
        }
    }
    return $true
}

$overallPassed = $true
foreach ($selectedMode in $selectedModes) {
    $port = if ($selectedMode -eq 'Listen') { $ListenPort } else { $DedicatedPort }
    $serverLog = Join-Path $logDirectory "Day02-$selectedMode-Server.log"
    $client1Log = Join-Path $logDirectory "Day02-$selectedMode-Client1.log"
    $client2Log = Join-Path $logDirectory "Day02-$selectedMode-Client2.log"
    $reportPath = Join-Path $reportDirectory "Day02-$selectedMode.json"
    $artifactPaths = @($serverLog, $client1Log, $client2Log)
    $managedProcesses = @()
    $modeResult = [ordered]@{
        SchemaVersion = 1
        Revision = $revision
        Mode = $selectedMode
        ServerRuntime = 'EditorServer'
        Port = $port
        StartedUtc = [DateTime]::UtcNow.ToString('o')
        CompletedUtc = $null
        Commands = @()
        Processes = @()
        Artifacts = [ordered]@{
            ServerLog = $serverLog
            Client1Log = $client1Log
            Client2Log = $client2Log
            Report = $reportPath
        }
        Assertions = [ordered]@{}
        Passed = $false
        Failure = $null
    }

    foreach ($artifactPath in @($artifactPaths + $reportPath)) {
        if (Test-Path -LiteralPath $artifactPath) {
            Remove-Item -LiteralPath $artifactPath -Force
        }
    }

    try {
        $serverMap = if ($selectedMode -eq 'Listen') { '/Game/Maps/StartupMap?listen' } else { '/Game/Maps/StartupMap' }
        if ($selectedMode -eq 'Dedicated') {
            $serverExecutable = Get-DedicatedServerExecutable
            if (Test-Path -LiteralPath $cookedStartupMap) {
                if ([string]::IsNullOrWhiteSpace($serverExecutable)) {
                    throw 'The cooked StartupMap exists but AuraServer executable was not found. Run BuildDedicatedServer.bat.'
                }
                $modeResult.ServerRuntime = 'PackagedDedicated'
                $serverArguments = @(
                    $serverMap,
                    '-server',
                    '-unattended',
                    '-nop4',
                    '-nullrhi',
                    '-nosound',
                    '-NoSplash',
                    "-port=$port",
                    "-abslog=$serverLog"
                )
            }
            else {
                Write-Warning 'Cooked StartupMap is unavailable; using UnrealEditor-Cmd.exe -server for the Day 2 Dedicated smoke.'
                $serverExecutable = $editorExe
                $serverArguments = @(
                    $projectFile,
                    $serverMap,
                    '-server',
                    '-unattended',
                    '-nop4',
                    '-nullrhi',
                    '-nosound',
                    '-NoSplash',
                    "-port=$port",
                    "-abslog=$serverLog"
                )
            }
        }
        else {
            $serverExecutable = $editorExe
            $serverArguments = @(
                $projectFile,
                $serverMap,
                '-server',
                '-unattended',
                '-nop4',
                '-nullrhi',
                '-nosound',
                '-NoSplash',
                "-port=$port",
                "-abslog=$serverLog"
            )
        }
        $modeResult.Commands += [ordered]@{
            Name = 'Server'
            Executable = $serverExecutable
            Arguments = @($serverArguments)
        }
        $server = Start-ManagedProcess -Name 'Server' -Executable $serverExecutable -Arguments $serverArguments
        $managedProcesses += $server

        $startupDeadline = [DateTime]::UtcNow.AddSeconds($StartupTimeoutSeconds)
        do {
            $serverStatus = Get-ManagedProcessStatus -Process $server.Process
            if ($serverStatus -eq 'Exited') {
                throw "Day 2 $selectedMode server exited early with code $($server.Process.ExitCode)."
            }
            if (Test-Path -LiteralPath $serverLog) {
                break
            }
            Start-Sleep -Milliseconds 250
        } while ([DateTime]::UtcNow -lt $startupDeadline)

        if (-not (Test-Path -LiteralPath $serverLog)) {
            throw "Day 2 $selectedMode server did not create '$serverLog' within the startup timeout."
        }

        $clientArguments = @(
            $projectFile,
            "127.0.0.1:$port",
            '-game',
            '-unattended',
            '-nop4',
            '-nullrhi',
            '-nosound',
            '-NoSplash'
        )

        foreach ($clientName in @('Client1', 'Client2')) {
            $clientLog = if ($clientName -eq 'Client1') { $client1Log } else { $client2Log }
            $clientArgumentsForProcess = @($clientArguments + "-abslog=$clientLog")
            $modeResult.Commands += [ordered]@{
                Name = $clientName
                Executable = $editorExe
                Arguments = @($clientArgumentsForProcess)
            }
            $client = Start-ManagedProcess -Name $clientName -Executable $editorExe -Arguments $clientArgumentsForProcess
            $managedProcesses += $client
            Start-Sleep -Milliseconds 500
        }

        $assertionDeadline = [DateTime]::UtcNow.AddSeconds($AssertionTimeoutSeconds)
        do {
            foreach ($managedProcess in $managedProcesses) {
                $processStatus = Get-ManagedProcessStatus -Process $managedProcess.Process
                if ($processStatus -ne 'Running') {
                    throw "Day 2 $selectedMode $($managedProcess.Record.Name) is no longer running before the assertions completed (status=$processStatus, code=$($managedProcess.Process.ExitCode))."
                }
            }

            $modeResult.Assertions = Get-NetworkAssertions -ServerLog $serverLog -Client1Log $client1Log -Client2Log $client2Log
            if (Test-AllAssertions -Assertions $modeResult.Assertions) {
                break
            }
            Start-Sleep -Seconds 1
        } while ([DateTime]::UtcNow -lt $assertionDeadline)

        if (-not (Test-AllAssertions -Assertions $modeResult.Assertions)) {
            $failedAssertions = @($modeResult.Assertions.GetEnumerator() | Where-Object { -not [bool]$_.Value } | ForEach-Object { $_.Key }) -join ', '
            throw "Timed out waiting for Day 2 $selectedMode assertions: $failedAssertions."
        }
    }
    catch {
        $modeResult.Failure = $_.Exception.Message
        $overallPassed = $false
    }
    finally {
        foreach ($managedProcess in $managedProcesses) {
            $processStatus = Get-ManagedProcessStatus -Process $managedProcess.Process
            if ($processStatus -eq 'Exited') {
                $managedProcess.Record.Status = 'Exited'
                try {
                    $managedProcess.Record.ExitCode = $managedProcess.Process.ExitCode
                }
                catch {
                    $managedProcess.Record.ExitCode = $null
                }
                if ([string]::IsNullOrWhiteSpace([string]$modeResult.Failure)) {
                    $modeResult.Failure = "Day 2 $selectedMode $($managedProcess.Record.Name) exited before teardown."
                }
                $overallPassed = $false
            }
        }

        foreach ($managedProcess in @($managedProcesses | Sort-Object { $_.Record.Name } -Descending)) {
            Stop-ManagedProcess -ManagedProcess $managedProcess -TimeoutSeconds $TeardownTimeoutSeconds
        }

        $modeResult.Assertions = Get-NetworkAssertions -ServerLog $serverLog -Client1Log $client1Log -Client2Log $client2Log
        $missingArtifacts = @($artifactPaths | Where-Object { -not (Test-Path -LiteralPath $_) })
        if ($missingArtifacts.Count -gt 0) {
            $artifactFailure = "Missing required artifact(s): $($missingArtifacts -join ', ')"
            if ([string]::IsNullOrWhiteSpace([string]$modeResult.Failure)) {
                $modeResult.Failure = $artifactFailure
            }
            else {
                $modeResult.Failure = "$($modeResult.Failure) $artifactFailure"
            }
            $overallPassed = $false
        }

        if (@($managedProcesses | Where-Object { $_.Record.Status -eq 'TeardownTimeout' }).Count -gt 0) {
            $modeResult.Failure = if ([string]::IsNullOrWhiteSpace([string]$modeResult.Failure)) {
                'One or more owned processes did not stop within the teardown timeout.'
            }
            else {
                "$($modeResult.Failure) One or more owned processes did not stop within the teardown timeout."
            }
            $overallPassed = $false
        }

        $modeResult.Processes = @($managedProcesses | ForEach-Object { $_.Record })
        $modeResult.CompletedUtc = [DateTime]::UtcNow.ToString('o')
        $modeResult.Passed = [string]::IsNullOrWhiteSpace([string]$modeResult.Failure) -and (Test-AllAssertions -Assertions $modeResult.Assertions) -and ($missingArtifacts.Count -eq 0)
        $modeResult | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $reportPath -Encoding UTF8
        if ($modeResult.Passed) {
            Write-Host "[Day2NetworkSmoke][$selectedMode] PASS"
        }
        else {
            Write-Host "[Day2NetworkSmoke][$selectedMode] FAIL: $($modeResult.Failure)"
        }
    }
}

if (-not $overallPassed) {
    exit 1
}

Write-Host "[Day2NetworkSmoke] PASS: $($selectedModes -join ', ') modes completed with two clients and reports in $reportDirectory."
exit 0
