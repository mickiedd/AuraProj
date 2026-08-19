[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)] [ValidateSet('Listen', 'Dedicated')] [string]$Mode,
    [string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }),
    [int]$Port = 17890,
    [int]$TimeoutSeconds = 90
)
& (Join-Path $PSScriptRoot 'RunRoleBattleCivilianNetworkSmoke.ps1') -Day 8 -Mode $Mode -EngineRoot $EngineRoot -Port $Port -TimeoutSeconds $TimeoutSeconds
exit $LASTEXITCODE
