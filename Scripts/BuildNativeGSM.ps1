param([string]$Configuration = 'Release')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio/Installer/vswhere.exe'
if (!(Test-Path $vswhere)) { throw 'Install Visual Studio Desktop development with C++ and CMake.' }
$vs = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$vs) { throw 'Visual Studio C++ toolchain not found.' }
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (!(Test-Path $cmake)) { $cmake = (Get-Command cmake -ErrorAction Stop).Source }
$version = & $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationVersion
$major = [int]($version.Split('.')[0])
$generator = if ($major -eq 16) { 'Visual Studio 16 2019' } elseif ($major -eq 17) { 'Visual Studio 17 2022' } else { throw "Unsupported Visual Studio version: $version" }
& $cmake -S "$root/Tools/GSM" -B "$root/Saved/GSMNative/build" -G $generator -A x64
if ($LASTEXITCODE) { throw 'GSM configure failed.' }
& $cmake --build "$root/Saved/GSMNative/build" --config $Configuration --parallel
if ($LASTEXITCODE) { throw 'GSM build failed.' }
Write-Host "Built $root/Saved/GSMNative/build/$Configuration/AuraGSM.exe"
