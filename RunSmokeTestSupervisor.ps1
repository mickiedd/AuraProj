[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$EditorPath,
    [Parameter(Mandatory = $true)] [string]$ProjectPath,
    [Parameter(Mandatory = $true)] [string]$LogPath,
    [string]$WorkingDirectory = (Split-Path -Parent $ProjectPath),
    [ValidateRange(30, 900)] [int]$TimeoutSeconds = 180
)

$ErrorActionPreference = 'Stop'

if (-not (Test-Path -LiteralPath $EditorPath -PathType Leaf)) { throw "UnrealEditor.exe was not found at '$EditorPath'." }
if (-not (Test-Path -LiteralPath $ProjectPath -PathType Leaf)) { throw "Aura.uproject was not found at '$ProjectPath'." }
if (-not (Test-Path -LiteralPath $WorkingDirectory -PathType Container)) { throw "Working directory was not found at '$WorkingDirectory'." }

$LogDirectory = Split-Path -Parent $LogPath
New-Item -ItemType Directory -Path $LogDirectory -Force | Out-Null
if (Test-Path -LiteralPath $LogPath) { Remove-Item -LiteralPath $LogPath -Force }

$StartInfo = New-Object System.Diagnostics.ProcessStartInfo
$StartInfo.FileName = $EditorPath
$StartInfo.WorkingDirectory = $WorkingDirectory
$StartInfo.UseShellExecute = $false
$StartInfo.CreateNoWindow = $true
$QuotedProjectPath = '"' + $ProjectPath.Replace('"', '\"') + '"'
$QuotedLogPath = '"' + $LogPath.Replace('"', '\"') + '"'
$StartInfo.Arguments = "$QuotedProjectPath -AuraAbilityGraphSmokeTest -nullrhi -log -abslog=$QuotedLogPath"
$Process = [System.Diagnostics.Process]::Start($StartInfo)
if ($null -eq $Process) { throw 'Failed to start UnrealEditor.exe.' }

try {
    if (-not $Process.WaitForExit($TimeoutSeconds * 1000)) {
        & taskkill.exe /PID $Process.Id /T /F *> $null
        Write-Error "AuraAbilityGraph smoke test timed out after $TimeoutSeconds seconds (process $($Process.Id))."
        exit 124
    }

    $ExitCode = $Process.ExitCode
    if (-not (Test-Path -LiteralPath $LogPath -PathType Leaf)) {
        Write-Error "AuraAbilityGraph smoke test produced no result log at '$LogPath'."
        exit 2
    }

    $ResultLine = Select-String -LiteralPath $LogPath -Pattern '\[SmokeTest\] Result: (\d+) passed, (\d+) failed\.' | Select-Object -Last 1
    if ($null -eq $ResultLine -or $ResultLine.Line -notmatch '\[SmokeTest\] Result: (\d+) passed, (\d+) failed\.') {
        Write-Error 'AuraAbilityGraph smoke test did not produce a parseable result line.'
        exit 3
    }
    $PassedCount = [int]$Matches[1]
    $FailedCount = [int]$Matches[2]
    if (-not (Select-String -LiteralPath $LogPath -Pattern '\[SmokeTest\] Requesting editor exit with status' -Quiet)) {
        Write-Error 'AuraAbilityGraph smoke test did not record its explicit exit request.'
        exit 4
    }
    if ($ExitCode -ne 0) {
        Write-Error "AuraAbilityGraph smoke test exited with status $ExitCode ($PassedCount passed, $FailedCount failed)."
        exit $ExitCode
    }
    if ($FailedCount -ne 0) {
        Write-Error "AuraAbilityGraph smoke test reported $FailedCount failed test(s) despite a zero process status."
        exit 1
    }

    Write-Host "AuraAbilityGraph smoke test passed: $PassedCount passed, $FailedCount failed."
    exit 0
}
finally {
    $Process.Dispose()
}
