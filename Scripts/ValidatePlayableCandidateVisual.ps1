[CmdletBinding()]
param(
    [string]$RunId = '',
    [switch]$AsJson
)

$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if ([string]::IsNullOrWhiteSpace($RunId)) { $RunId = "visual-$(Get-Date -Format 'yyyyMMdd-HHmmss')" }
$pages = @(
    'Plugins/AuraWebUI/Content/WebUI/hud-left-top.html',
    'Plugins/AuraWebUI/Content/WebUI/hud-right-top.html',
    'Plugins/AuraWebUI/Content/WebUI/hud-bottom.html',
    'Plugins/AuraWebUI/Content/WebUI/hud-interaction.html'
)
$checks = [System.Collections.Generic.List[object]]::new()
foreach ($page in $pages) {
    $path = Join-Path $repoRoot $page
    $text = if (Test-Path -LiteralPath $path) { Get-Content -LiteralPath $path -Raw } else { '' }
    $required = if ($page -like '*right-top*') { @('firearmPanel','hud_firearm','reloadFirearm','hud_merchant') } elseif ($page -like '*left-top*') { @('tutorialState','hud_tutorial','hud_role_state') } elseif ($page -like '*interaction*') { @('hud_interaction','hud_interaction_cleared','state.selectedOption=-1') } else { @('skill_ability_pressed','skill_ability_released') }
    foreach ($token in $required) { $checks.Add([pscustomobject]@{ page = $page; token = $token; passed = $text.Contains($token) }) }
}
$result = [ordered]@{ schemaVersion = 1; runId = $RunId; checks = @($checks); passed = (@($checks | Where-Object { -not $_.passed }).Count -eq 0) }
if ($AsJson) { $result | ConvertTo-Json -Depth 10; exit $(if ($result.passed) { 0 } else { 1 }) }
$result | ConvertTo-Json -Depth 10
exit $(if ($result.passed) { 0 } else { 1 })
