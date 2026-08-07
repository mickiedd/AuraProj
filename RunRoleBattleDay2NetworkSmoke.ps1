param(
    [string]$EngineRoot = $env:UE_ENGINE_ROOT,
    [int]$Port = 17777,
    [int]$TimeoutSeconds = 35
)

$ErrorActionPreference = 'Stop'
$projectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$projectFile = Join-Path $projectRoot 'Aura.uproject'
$logDirectory = Join-Path $projectRoot 'Saved\Logs'
$serverLog = Join-Path $logDirectory 'Day2NetworkServer.log'
$clientLog = Join-Path $logDirectory 'Day2NetworkClient.log'

if ([string]::IsNullOrWhiteSpace($EngineRoot)) {
    Write-Error "UE_ENGINE_ROOT is not set. Set it to the Unreal Engine root."
    exit 1
}

$editorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
if (-not (Test-Path -LiteralPath $editorExe)) {
    Write-Error "UnrealEditor-Cmd.exe was not found. Set UE_ENGINE_ROOT to the Unreal Engine root."
    exit 1
}
if (-not (Test-Path -LiteralPath $projectFile)) {
    Write-Error "Aura.uproject was not found at '$projectFile'."
    exit 1
}

New-Item -ItemType Directory -Path $logDirectory -Force | Out-Null
foreach ($oldLog in @($serverLog, $clientLog)) {
    if (Test-Path -LiteralPath $oldLog) {
        Remove-Item -LiteralPath $oldLog -Force
    }
}

function Test-LogPattern {
    param([string]$Path, [string]$Pattern)
    if (-not (Test-Path -LiteralPath $Path)) {
        return $false
    }
    return [bool](Select-String -LiteralPath $Path -Pattern $Pattern -Quiet)
}

$serverProcess = $null
$clientProcess = $null
$passed = $false

try {
    $serverArguments = @(
        $projectFile,
        '/Game/Maps/StartupMap?listen',
        '-server',
        '-unattended',
        '-nop4',
        '-nullrhi',
        '-nosound',
        '-NoSplash',
        "-port=$Port",
        "-abslog=$serverLog"
    )
    $serverProcess = Start-Process -FilePath $editorExe -ArgumentList $serverArguments -PassThru -WindowStyle Hidden
    Start-Sleep -Seconds 8
    if ($serverProcess.HasExited) {
        throw "Day 2 smoke server exited early with code $($serverProcess.ExitCode)."
    }

    $clientArguments = @(
        $projectFile,
        "127.0.0.1:$Port",
        '-game',
        '-unattended',
        '-nop4',
        '-nullrhi',
        '-nosound',
        '-NoSplash',
        "-abslog=$clientLog"
    )
    $clientProcess = Start-Process -FilePath $editorExe -ArgumentList $clientArguments -PassThru -WindowStyle Hidden

    $deadline = [DateTime]::UtcNow.AddSeconds($TimeoutSeconds)
    do {
        if ($serverProcess.HasExited) {
            throw "Day 2 smoke server exited early with code $($serverProcess.ExitCode)."
        }
        if ($clientProcess.HasExited) {
            throw "Day 2 smoke client exited early with code $($clientProcess.ExitCode)."
        }

        $joined = Test-LogPattern $serverLog 'Join succeeded:'
        $serverPlayer = Test-LogPattern $serverLog '\[CombatIdentity\]\[Server\].*Faction=Faction\.Player.*Control=Control\.Player.*Profile=Combat\.Unassigned.*Death=Death\.PlayerRespawn.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        $serverEnemy = Test-LogPattern $serverLog '\[CombatIdentity\]\[Server\].*Faction=Faction\.Enemy.*Control=Control\.EnemyAI.*Profile=Combat\.Unassigned.*Death=Death\.EnemyLoot.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        $clientPlayer = Test-LogPattern $clientLog '\[CombatIdentity\]\[Client\].*Faction=Faction\.Player.*Control=Control\.Player.*Profile=Combat\.Unassigned.*Death=Death\.PlayerRespawn.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        $clientEnemy = Test-LogPattern $clientLog '\[CombatIdentity\]\[Client\].*Faction=Faction\.Enemy.*Control=Control\.EnemyAI.*Profile=Combat\.Unassigned.*Death=Death\.EnemyLoot.*Targetable=1 CanAttack=1 CanBeDamaged=1 FriendlyFire=0'
        $aiTarget = Test-LogPattern $serverLog '\[EnemyAI\]\[FindNearestPlayer\].*Candidates=1.*Closest=BP_AuraCharacter'
        $passed = $joined -and $serverPlayer -and $serverEnemy -and $clientPlayer -and $clientEnemy -and $aiTarget
        if (-not $passed) {
            Start-Sleep -Seconds 1
        }
    } while (-not $passed -and [DateTime]::UtcNow -lt $deadline)

    if (-not $passed) {
        throw "Timed out waiting for replicated Player/Enemy identities and identity-based AI targeting."
    }
}
finally {
    if ($clientProcess -and -not $clientProcess.HasExited) {
        Stop-Process -Id $clientProcess.Id -Force
    }
    if ($serverProcess -and -not $serverProcess.HasExited) {
        Stop-Process -Id $serverProcess.Id -Force
    }
}

Write-Host '[Day2NetworkSmoke] PASS: client received matching Player/Enemy identities and Enemy AI acquired the Player through identity.'
Write-Host "[Day2NetworkSmoke] Server log: $serverLog"
Write-Host "[Day2NetworkSmoke] Client log: $clientLog"
exit 0
