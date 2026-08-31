#Requires -Version 7.0
[CmdletBinding()]
param(
    [string]$ScopePath,
    [string]$BaselineEvidencePath,
    [string]$EvidenceRoot,
    [string]$PackageRoot,
    [string]$OutputPath,
    [switch]$AsJson
)
$ErrorActionPreference = 'Stop'
$repoRoot = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
if (-not $ScopePath) { $ScopePath = Join-Path $repoRoot 'Docs/Plans/Gameplay-Expansion-Implementation/gameplay-expansion-scope.json' }
$arguments = @((Join-Path $PSScriptRoot 'gameplay_expansion.py'), 'audit', '--repo-root', $repoRoot, '--scope', $ScopePath)
if ($BaselineEvidencePath) { $arguments += @('--baseline-evidence', $BaselineEvidencePath) }
if ($EvidenceRoot) { $arguments += @('--evidence-root', $EvidenceRoot) }
if ($PackageRoot) { $arguments += @('--package-root', $PackageRoot) }
if ($OutputPath) { $arguments += @('--output', $OutputPath) }
# Python owns strict JSON validation. Do not round-trip untrusted input through
# ConvertFrom-Json, which would discard duplicate-key evidence before validation.
$python = @(Get-Command python -CommandType Application -ErrorAction Stop)[0].Source
& $python @arguments
$auditExit = $LASTEXITCODE
exit $auditExit
