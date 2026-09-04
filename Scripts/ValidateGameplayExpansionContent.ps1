param([string]$RepoRoot = '.', [string]$Manifest = 'Content/Config/GameplayExpansionManifest.json', [string]$StagedRoot)
$ErrorActionPreference = 'Stop'
$args = @('Scripts/validate_gameplay_expansion_content.py','--repo-root',$RepoRoot,'--manifest',$Manifest)
if ($StagedRoot) { $args += @('--staged-root',$StagedRoot) }
& python @args
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
