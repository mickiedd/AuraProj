[CmdletBinding()]
param([string]$EngineRoot = $(if ($env:UE_ENGINE_ROOT) { $env:UE_ENGINE_ROOT } else { 'C:\Git\UnrealEngine-5.5' }))
& (Join-Path $PSScriptRoot 'RunRoleBattleDays789Automation.ps1') -Day 7 -Mode Listen -EngineRoot $EngineRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
& (Join-Path $PSScriptRoot 'RunRoleBattleDay6NetworkSmoke.ps1') -Mode Listen -EngineRoot $EngineRoot
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$ServerLog = Join-Path (Split-Path -Parent $MyInvocation.MyCommand.Path) 'Saved\Logs\Day06-Listen-Server.log'
$AuraGrant = (Test-Path -LiteralPath $ServerLog) -and [bool](Select-String -LiteralPath $ServerLog -Pattern '\[RoleGrant\]\[Server\] Role=Aura Required=4 OwnedSpecs=4' -Quiet)
$BungeeGrant = (Test-Path -LiteralPath $ServerLog) -and [bool](Select-String -LiteralPath $ServerLog -Pattern '\[RoleGrant\]\[Server\] Role=BungeeMan Required=1 OwnedSpecs=1' -Quiet)
if (-not (Test-Path -LiteralPath $ServerLog) -or -not $AuraGrant -or -not $BungeeGrant) {
    Write-Error 'Day 7 listen topology did not prove the authoritative Aura/BungeeMan grant sets.'
    exit 1
}
Write-Host '[RoleBattleDay7][Listen] PASS'
exit 0
