[CmdletBinding()]
param(
    [string]$SourceRoot = 'C:\Works\Crunch-master',
    [string]$ProjectRoot = ''
)

$ErrorActionPreference = 'Stop'

if ([string]::IsNullOrWhiteSpace($ProjectRoot)) {
    $ProjectRoot = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
}

$sourceContent = Join-Path $SourceRoot 'Content'
$targetContent = Join-Path $ProjectRoot 'Content'
$sourcePrefix = '/Game/ParagonCrunch/'
$targetPrefix = '/Game/Assets/Crunch/'

if ([Text.Encoding]::ASCII.GetByteCount($sourcePrefix) -ne [Text.Encoding]::ASCII.GetByteCount($targetPrefix)) {
    throw 'Source and target package prefixes must have the same byte length.'
}

if (-not (Test-Path -LiteralPath $sourceContent -PathType Container)) {
    throw "Crunch source content was not found: $sourceContent"
}

if (-not (Get-Command rg -ErrorAction SilentlyContinue)) {
    throw 'ripgrep (rg) is required to scan Unreal package references.'
}

$seedRelativePaths = @(
    'ParagonCrunch\Characters\Heroes\Crunch\Materials\MaterialClasses\M_Crunch_Hoses.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Materials\MaterialClasses\M_Crunch_Pistons.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Materials\MIC_Crunch_Body.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Materials\MIC_Crunch_Fists.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Materials\MIC_Crunch_Head.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Materials\MIC_Crunch_Legs.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Materials\MIC_Crunch_Torso.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Meshes\Crunch_Extents.uasset',
    'ParagonCrunch\Characters\Heroes\Crunch\Meshes\Crunch_ShadowCyl.uasset'
)

function Get-ParagonCrunchReferences([string]$Path) {
    & rg -a -o --no-filename '/Game/ParagonCrunch/[A-Za-z0-9_./-]+' -- $Path 2>$null |
        ForEach-Object {
            if ($_ -match '^(/Game/[^.]+)') {
                $matches[1]
            }
        } |
        Sort-Object -Unique
}

function Replace-AsciiPackagePrefix([string]$Path) {
    # ISO-8859-1 gives a one-to-one byte/string mapping, so replacing the ASCII
    # package prefix cannot rewrite unrelated binary package data. String.Replace
    # is implemented in native .NET code and is materially faster than scanning
    # large Unreal packages in an interpreted PowerShell loop.
    $byteEncoding = [Text.Encoding]::GetEncoding(28591)
    $bytes = [IO.File]::ReadAllBytes($Path)
    $text = $byteEncoding.GetString($bytes)
    $updated = $text.Replace($sourcePrefix, $targetPrefix)
    if ($updated -ne $text) {
        [IO.File]::WriteAllBytes($Path, $byteEncoding.GetBytes($updated))
    }
}

$queue = [Collections.Generic.Queue[string]]::new()
$seen = [Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
$relativePaths = [Collections.Generic.List[string]]::new()
foreach ($seed in $seedRelativePaths) {
    $queue.Enqueue($seed)
}

while ($queue.Count -gt 0) {
    $relativePath = $queue.Dequeue()
    if (-not $seen.Add($relativePath)) {
        continue
    }

    $sourcePath = Join-Path $sourceContent $relativePath
    if (-not (Test-Path -LiteralPath $sourcePath -PathType Leaf)) {
        throw "Missing source dependency: $sourcePath"
    }

    $relativePaths.Add($relativePath)
    foreach ($reference in (Get-ParagonCrunchReferences $sourcePath)) {
        $dependencyRelativePath = ($reference.Substring(6).Replace('/', '\')) + '.uasset'
        if (-not $seen.Contains($dependencyRelativePath)) {
            $queue.Enqueue($dependencyRelativePath)
        }
    }
}

$copiedBytes = [int64]0
foreach ($relativePath in $relativePaths) {
    $sourcePath = Join-Path $sourceContent $relativePath
    $targetRelativePath = $relativePath -replace '^ParagonCrunch', 'Assets\Crunch'
    $targetPath = Join-Path $targetContent $targetRelativePath
    $targetDirectory = Split-Path -Parent $targetPath
    New-Item -ItemType Directory -Path $targetDirectory -Force | Out-Null
    Copy-Item -LiteralPath $sourcePath -Destination $targetPath -Force

    Replace-AsciiPackagePrefix $targetPath
    $copiedBytes += (Get-Item -LiteralPath $targetPath).Length
}

$repairTargets = @(
    Get-ChildItem (Join-Path $targetContent 'Assets\Characters\Crunch\Meshes') -File -Filter 'SM_Crunch*.uasset'
    Get-ChildItem (Join-Path $targetContent 'Assets\Characters\Crunch\Meshes') -File -Filter 'Crunch_Skeleton*.uasset'
    Get-Item (Join-Path $targetContent 'Assets\Characters\Crunch\Animations\Abilities\Anim_Tornado_Aura.uasset') -ErrorAction SilentlyContinue
) | Where-Object { $_ -and (Test-Path -LiteralPath $_.FullName -PathType Leaf) }

foreach ($target in $repairTargets) {
    Replace-AsciiPackagePrefix $target.FullName
}

$reportDirectory = Join-Path $ProjectRoot 'Saved\Reports\CrunchMigration'
New-Item -ItemType Directory -Path $reportDirectory -Force | Out-Null
$remainingReferences = [Collections.Generic.List[string]]::new()
foreach ($target in $repairTargets) {
    foreach ($reference in (Get-ParagonCrunchReferences $target.FullName)) {
        $remainingReferences.Add($reference)
    }
}
$report = [ordered]@{
    schemaVersion = 1
    sourceRoot = $SourceRoot
    targetPrefix = $targetPrefix
    packageCount = $relativePaths.Count
    copiedBytes = $copiedBytes
    repairedTargetCount = $repairTargets.Count
    sourceReferencesRemainingInRepairedTargets = @($remainingReferences | Sort-Object -Unique)
}
$report | ConvertTo-Json -Depth 4 | Set-Content -LiteralPath (Join-Path $reportDirectory 'crunch-mesh-texture-repair.json') -Encoding UTF8

Write-Output ("Crunch dependency repair complete: packages={0}; copiedMiB={1:N1}; repairedTargets={2}" -f $relativePaths.Count, ($copiedBytes / 1MB), $repairTargets.Count)
