[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet('7', '8', '9')]
    [string]$Day,
    [ValidateSet('Listen', 'Dedicated', 'Packaged')]
    [string]$Mode = 'Dedicated',
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [string]$ArchivePath,
    [ValidateRange(30, 900)]
    [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'
$ProjectRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$ProjectFile = Join-Path $ProjectRoot 'Aura.uproject'
$EditorExe = Join-Path $EngineRoot 'Engine\Binaries\Win64\UnrealEditor-Cmd.exe'
$LogDirectory = Join-Path $ProjectRoot 'Saved\Logs'
$ReportDirectory = Join-Path $ProjectRoot 'Saved\Reports'
$LogPath = Join-Path $LogDirectory "Day$Day-$Mode-Automation.log"
$ReportPath = Join-Path $ReportDirectory "Day$Day-$Mode.json"

if (-not (Test-Path -LiteralPath $EditorExe)) { throw "UnrealEditor-Cmd.exe was not found at '$EditorExe'." }
if (-not (Test-Path -LiteralPath $ProjectFile)) { throw "Aura.uproject was not found at '$ProjectFile'." }
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
New-Item -ItemType Directory -Path $ReportDirectory -Force | Out-Null
foreach ($Artifact in @($LogPath, $ReportPath)) {
    if (Test-Path -LiteralPath $Artifact) { Remove-Item -LiteralPath $Artifact -Force }
}

$StartedUtc = [DateTime]::UtcNow.ToString('o')
$Assertions = [ordered]@{}
$Passed = $false
$Failure = $null
$ProcessId = $null
$ExitCode = $null

try {
    $Python = Get-Command python.exe -ErrorAction SilentlyContinue
    if (-not $Python) { throw 'python.exe is required for the focused Day 7-9 contract checks.' }
    & $Python.Source (Join-Path $ProjectRoot 'Scripts\test_role_battle_days_7_9.py')
    if ($LASTEXITCODE -ne 0) { throw 'Focused Day 7-9 data/source contracts failed.' }
    $Assertions.StaticContracts = $true

    if ($Mode -eq 'Packaged') {
        if ([string]::IsNullOrWhiteSpace($ArchivePath) -or -not (Test-Path -LiteralPath $ArchivePath)) {
            throw 'Packaged mode requires an existing -ArchivePath from BuildCookRun.'
        }
        $LogPath = Join-Path $LogDirectory 'Day07-Packaged-Server.log'
        & (Join-Path $ProjectRoot 'RunRoleBattleDay7PackagedTopology.ps1') -ArchivePath $ArchivePath -EngineRoot $EngineRoot -TimeoutSeconds $TimeoutSeconds
        if ($LASTEXITCODE -ne 0) { throw 'Packaged topology runner failed.' }
        $Assertions.PackagedTopology = $true
        $Passed = $true
    }
    else {
        $AutomationName = "Aura.RoleBattle.Day$Day"
        $ArgumentList = @(
            $ProjectFile, '-unattended', '-nop4', '-nullrhi', '-nosound', '-NoSplash',
            ('-ExecCmds="Automation RunTests ' + $AutomationName + '; Quit"'),
            '-TestExit="Automation Test Queue Empty"', "-abslog=$LogPath"
        )
        $Process = Start-Process -FilePath $EditorExe -ArgumentList $ArgumentList -PassThru -WindowStyle Hidden
        $ProcessId = $Process.Id
        if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
            Stop-Process -Id $Process.Id -Force -ErrorAction SilentlyContinue
            throw "Automation process timed out after $TimeoutSeconds seconds."
        }
        $ExitCode = $Process.ExitCode
        if ($ExitCode -ne 0) { throw "Automation process exited with code $ExitCode." }
        if (-not (Test-Path -LiteralPath $LogPath)) { throw "Automation log was not written: $LogPath" }
        $LogText = Get-Content -Raw -LiteralPath $LogPath
        $ExpectedTestCount = switch ($Day) {
            '7' { 10 }
            '8' { 6 }
            '9' { 9 }
        }
        $SuccessCount = ([regex]::Matches($LogText, 'Result=\{Success\}')).Count
        if ($SuccessCount -ne $ExpectedTestCount) {
            throw "Automation log recorded $SuccessCount successful tests; expected $ExpectedTestCount."
        }
        if (-not $LogText.Contains('**** TEST COMPLETE. EXIT CODE: 0 ****')) {
            throw 'Automation log did not record a zero test exit code.'
        }
        $Assertions.ExpectedTestResults = "$SuccessCount/$ExpectedTestCount"
        $Assertions.NativeAutomationProcess = $true
        $Passed = $true
    }
}
catch {
    $Failure = $_.Exception.Message
}
finally {
    $Report = [ordered]@{
        SchemaVersion = 1
        Day = $Day
        Mode = $Mode
        StartedUtc = $StartedUtc
        CompletedUtc = [DateTime]::UtcNow.ToString('o')
        ProcessId = $ProcessId
        ExitCode = $ExitCode
        Assertions = $Assertions
        Artifacts = [ordered]@{ Log = $LogPath; Report = $ReportPath; ArchivePath = $ArchivePath }
        Passed = $Passed
        Failure = $Failure
    }
    $Report | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath $ReportPath -Encoding UTF8
}

if (-not $Passed) { Write-Error "[RoleBattleDay$Day][$Mode] FAIL: $Failure"; exit 1 }
Write-Host "[RoleBattleDay$Day][$Mode] PASS"
exit 0
