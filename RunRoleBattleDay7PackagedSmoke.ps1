[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [string]$ArchivePath,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' })
)
& (Join-Path $PSScriptRoot 'RunRoleBattleDays789Automation.ps1') -Day 7 -Mode Packaged -ArchivePath $ArchivePath -EngineRoot $EngineRoot
exit $LASTEXITCODE
