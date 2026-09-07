[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
$batchSource = Join-Path $root 'RunClientNullRHI.bat'
$associatedProject = Join-Path $root 'Aura.uproject'
$fallbackProject = Join-Path $PSScriptRoot 'Fixtures\NullRhiLauncherUnresolvableAssociation.uproject'
$goodEditor = 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor.exe'
$alternateEditor = 'C:\Git\UnrealEngine-5.5\Engine\Binaries\Win64\UnrealEditor.exe'

foreach ($path in @($batchSource, $associatedProject, $fallbackProject, $goodEditor, $alternateEditor)) {
	if (-not (Test-Path -LiteralPath $path)) { throw "Required launcher test path is missing: $path" }
}

$tempRoot = Join-Path ([IO.Path]::GetTempPath()) ('AuraNullRhiLauncherTest-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tempRoot -Force | Out-Null

function New-LauncherCase([string]$Name, [string]$ProjectPath) {
	$caseRoot = Join-Path $tempRoot $Name
	New-Item -ItemType Directory -Path $caseRoot -Force | Out-Null
	Copy-Item -LiteralPath $batchSource -Destination (Join-Path $caseRoot 'RunClientNullRHI.bat')
	Copy-Item -LiteralPath $ProjectPath -Destination (Join-Path $caseRoot 'Aura.uproject')
	return $caseRoot
}

function Invoke-LauncherCase([string]$CaseRoot, [string]$Arguments, [AllowNull()][string]$EnvironmentEditor) {
	$batchPath = Join-Path $CaseRoot 'RunClientNullRHI.bat'
	$command = '"{0}" Scifi_Desert_Level {1}' -f $batchPath, $Arguments
	$hadEnvironmentEditor = Test-Path Env:UE_EDITOR_EXE
	$previousEnvironmentEditor = $env:UE_EDITOR_EXE
	try {
		if ([string]::IsNullOrWhiteSpace($EnvironmentEditor)) {
			Remove-Item Env:UE_EDITOR_EXE -ErrorAction SilentlyContinue
		} else {
			$env:UE_EDITOR_EXE = $EnvironmentEditor
		}
		$output = (& cmd.exe /d /c $command 2>&1 | Out-String)
		return [pscustomobject]@{
			ExitCode = $LASTEXITCODE
			Output = $output
		}
	} finally {
		if ($hadEnvironmentEditor) {
			$env:UE_EDITOR_EXE = $previousEnvironmentEditor
		} else {
			Remove-Item Env:UE_EDITOR_EXE -ErrorAction SilentlyContinue
		}
	}
}

function Assert-LauncherCase([string]$Name, [string]$ProjectPath, [string]$Arguments, [AllowNull()][string]$EnvironmentEditor, [int]$ExpectedExitCode, [string]$ExpectedText, [string]$ForbiddenText) {
	$caseRoot = New-LauncherCase $Name $ProjectPath
	$result = Invoke-LauncherCase $caseRoot $Arguments $EnvironmentEditor
	if ($result.ExitCode -ne $ExpectedExitCode) {
		throw "$Name expected exit $ExpectedExitCode but got $($result.ExitCode). Output:`n$($result.Output)"
	}
	if (-not $result.Output.Contains($ExpectedText)) {
		throw "$Name did not contain expected text '$ExpectedText'. Output:`n$($result.Output)"
	}
	if (-not [string]::IsNullOrEmpty($ForbiddenText) -and $result.Output.Contains($ForbiddenText)) {
		throw "$Name contained forbidden text '$ForbiddenText'. Output:`n$($result.Output)"
	}
	Write-Output ("PASS {0}: exit {1}, selected {2}" -f $Name, $result.ExitCode, $ExpectedText)
}

try {
	Assert-LauncherCase 'ExplicitOverrideWins' $associatedProject ('-editor-exe "{0}" -dry-run' -f $goodEditor) $alternateEditor 0 $goodEditor 'WARNING: EngineAssociation'
	Assert-LauncherCase 'EngineAssociationWithoutEnvironment' $associatedProject '-dry-run' $null 0 $goodEditor ''
	Assert-LauncherCase 'EngineAssociationBeatsStaleEnvironment' $associatedProject '-dry-run' $alternateEditor 0 $goodEditor $alternateEditor
	Assert-LauncherCase 'EnvironmentFallbackWhenAssociationMissing' $fallbackProject '-dry-run' $goodEditor 0 $goodEditor 'Could not find UnrealEditor.exe.'
	Assert-LauncherCase 'InvalidExplicitOverrideFailsClosed' $associatedProject '-editor-exe "C:\Missing\UnrealEditor.exe" -dry-run' $goodEditor 1 'Could not find the UnrealEditor.exe supplied by -editor-exe:' 'DRY RUN: editor launch suppressed.'
	Assert-LauncherCase 'MissingAssociationAndEnvironmentFailsClosed' $fallbackProject '-dry-run' $null 1 'Could not find UnrealEditor.exe.' 'DRY RUN: editor launch suppressed.'
	Write-Output 'RunClientNullRHI launcher precedence matrix PASS.'
	exit 0
} finally {
	if (Test-Path -LiteralPath $tempRoot) {
		Remove-Item -LiteralPath $tempRoot -Recurse -Force
	}
}
