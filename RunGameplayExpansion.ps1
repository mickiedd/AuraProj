#Requires -Version 7.0
[CmdletBinding()]
param(
    [ValidateRange(41,60)][int]$Day = 41,
    [ValidateSet('Preflight','Fast','Packaged','Playtest','Soak','Finalize')][string]$Stage = 'Preflight',
    [Parameter(Mandatory)][string]$RunId,
    [string]$ScopePath,
    [string]$BaselineEvidencePath,
    [string]$EvidenceRoot,
    [string]$PackageRoot,
    [string]$EngineRoot,
    [string]$PackageManifestPath,
    [string]$Topology = 'All',
    [string]$Composition = 'All',
    [string]$SeedSet = 'Core',
    [string]$SessionManifestPath,
    [int]$CyclesPerTopology = 10,
    [string]$TechnicalPath,
    [string]$PlaytestPath,
    [string]$PerformancePath,
    [string]$SoakPath,
    [string]$RegressionPath,
    [string]$Revision
)
$ErrorActionPreference = 'Stop'
$repoRoot = $PSScriptRoot
$clock = [Diagnostics.Stopwatch]::StartNew()
$started = [DateTime]::UtcNow
$processRecords = [Collections.Generic.List[object]]::new()
$exitCode = 2
$resultPath = $null
$result = $null
$implementationPaths = @(
    'RunGameplayExpansion.ps1',
    'Scripts/ValidateGameplayExpansionScope.ps1',
    'Scripts/GameplayExpansionProcess.cs',
    'Scripts/gameplay_expansion.py',
    'Scripts/validate_gameplay_expansion_content.py',
    'Scripts/gameplay_baseline_readers.py',
    'Scripts/gameplay_native_warnings.py',
    'Scripts/test_gameplay_expansion.py',
    'Scripts/test_gameplay_day57_contracts.py',
    'Scripts/test_gameplay_day58_contracts.py',
    'Scripts/test_gameplay_baseline_readers.py',
    'Scripts/test_gameplay_native_warnings.py',
    'Docs/Plans/Gameplay-Expansion-Implementation/gameplay-expansion-scope.json',
    'Source/AuraEditor/Private/AuraEditorModule.cpp',
    'Source/AuraEditor/Private/Tests/AuraBaselineAssetTests.cpp',
    'Source/Aura/Private/Character/AuraCharacterBase.cpp',
    'Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp',
    'Source/Aura/Private/Tests/Fixtures/AuraRoleApplicationTestActor.cpp',
    'Source/Aura/Private/Tests/Fixtures/AuraRoleApplicationTestActor.h',
    'Source/Aura/Private/Tests/AuraRoleBattleTests.cpp',
    'RunPlayableCandidate.ps1',
    'Scripts/test_playable_candidate.py'
)
$contentPaths = @('Docs/Plans/Playable-Candidate-Implementation/playable-candidate-scope.json', 'Content/Config/PlayableCandidateManifest.json',
    'Content/Config/GameplayExpansionManifest.json', 'Content/Config/GameplayPerformanceBudgets.json',
    'Content/MilitaryWeapDark/Sound/Rifle/Rifle_ImpactSurface_Cue.uasset', 'Content/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue.uasset')

function Write-JsonFile([string]$Path, $Value) {
    [IO.File]::WriteAllText($Path, ($Value | ConvertTo-Json -Depth 40), [Text.UTF8Encoding]::new($false))
}

function Get-InputHashes([string[]]$RelativePaths) {
    $hashes = [ordered]@{}
    foreach ($relative in $RelativePaths) {
        $path = Join-Path $repoRoot $relative
        $hashes[$relative] = $(if (Test-Path -LiteralPath $path -PathType Leaf) { (Get-FileHash -LiteralPath $path -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null })
    }
    return $hashes
}

function Assert-NoReparse([string]$Path) {
    $cursor = [IO.Path]::GetFullPath($Path)
    while ($cursor) {
        if (Test-Path -LiteralPath $cursor) {
            if ((Get-Item -LiteralPath $cursor -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
                throw "Unsafe reparse-point output/input root: $cursor"
            }
        }
        $parent = [IO.Path]::GetDirectoryName($cursor)
        if ($parent -eq $cursor) { break }
        $cursor = $parent
    }
}

function Invoke-Owned([string]$Name, [string]$Executable, [string[]]$Arguments, [int]$TimeoutSeconds) {
    $outPath = Join-Path $runDir "$Name.stdout.log"
    $errPath = Join-Path $runDir "$Name.stderr.log"
    $helperPath = Join-Path $repoRoot 'Scripts/GameplayExpansionProcess.cs'
    if (-not ('AuraGameplayExpansion.OwnedChild' -as [type])) { Add-Type -Path $helperPath }
    $stepClock = [Diagnostics.Stopwatch]::StartNew()
    $stepStart = [DateTime]::UtcNow
    $observedPids = [Collections.Generic.HashSet[int]]::new()
    $ownedChild = $null
    $timedOut = $false
    $cleanupFailure = $false
    $childExit = $null
    $record = [ordered]@{ name=$Name; executable=$Executable; executableSha256=(Get-FileHash -LiteralPath $Executable -Algorithm SHA256).Hash.ToLowerInvariant(); arguments=$Arguments; startedAtUtc=$stepStart.ToString('o'); exitCode=$null; status='FAIL'; cleanup='NOT_RUN'; stdout=$outPath; stderr=$errPath; ownership='WindowsJobObject'; processHelperSha256=(Get-FileHash -LiteralPath $helperPath -Algorithm SHA256).Hash.ToLowerInvariant(); totalProcessCount=0; remainingProcessCount=0 }
    try {
        $ownedChild = [AuraGameplayExpansion.OwnedChild]::Start($Executable, $Arguments, $repoRoot)
        $child = $ownedChild.Process
        [void]$observedPids.Add($child.Id)
        while (-not $child.WaitForExit(1000)) {
            foreach ($memberId in $ownedChild.ActiveProcessIds) { [void]$observedPids.Add($memberId) }
            if ([int]$stepClock.Elapsed.TotalSeconds -ge $TimeoutSeconds) { $timedOut = $true; break }
            if ([int]$stepClock.Elapsed.TotalSeconds % 15 -eq 0) { Write-Host ("{0}: running {1}s" -f $Name, [int]$stepClock.Elapsed.TotalSeconds) }
        }
        if ($timedOut) {
            $null = $child.CloseMainWindow()
            $null = $child.WaitForExit(15000)
        } else { $childExit = $child.ExitCode }
    } finally {
        if ($ownedChild) {
            try {
                $record.totalProcessCount = $ownedChild.TotalProcessCount
                foreach ($memberId in $ownedChild.ActiveProcessIds) { [void]$observedPids.Add($memberId) }
                if (-not $timedOut -and $ownedChild.ActiveProcessCount -gt 0) {
                    $drainClock = [Diagnostics.Stopwatch]::StartNew()
                    Write-Host ("{0}: waiting up to 15s for owned children to exit" -f $Name)
                    while ($ownedChild.ActiveProcessCount -gt 0 -and $drainClock.Elapsed.TotalSeconds -lt 15) { Start-Sleep -Milliseconds 100 }
                    $record.graceDrainSeconds = $drainClock.Elapsed.TotalSeconds
                }
                if ($ownedChild.ActiveProcessCount -gt 0) {
                    if (-not $timedOut) { $cleanupFailure = $true }
                    $ownedChild.Terminate()
                    $cleanupClock = [Diagnostics.Stopwatch]::StartNew()
                    while ($ownedChild.ActiveProcessCount -gt 0 -and $cleanupClock.Elapsed.TotalSeconds -lt 5) { Start-Sleep -Milliseconds 100 }
                }
                $record.remainingProcessCount = $ownedChild.ActiveProcessCount
                if ($record.remainingProcessCount -ne 0) { $cleanupFailure = $true }
            } catch { $cleanupFailure = $true; $record.cleanupError = $_.Exception.Message }
            try {
                if ($ownedChild.StandardOutput.Wait(5000)) { [IO.File]::WriteAllText($outPath, $ownedChild.StandardOutput.Result) } else { $cleanupFailure = $true }
                if ($ownedChild.StandardError.Wait(5000)) { [IO.File]::WriteAllText($errPath, $ownedChild.StandardError.Result) } else { $cleanupFailure = $true }
                if ($child.HasExited) { $childExit = $child.ExitCode }
            } catch { $cleanupFailure = $true; $record.captureError = $_.Exception.Message }
            finally {
                # The non-inherited job handle also guarantees cleanup if this
                # runner is cancelled or its process exits unexpectedly.
                $ownedChild.Dispose()
            }
        }
        $record.completedAtUtc = [DateTime]::UtcNow.ToString('o')
        $record.durationSeconds = $stepClock.Elapsed.TotalSeconds
        $record.exitCode = $childExit
        $record.observedOwnedPids = @($observedPids)
        $record.cleanup = $(if ($cleanupFailure) { 'FAIL' } else { 'PASS' })
        $record.status = $(if ($timedOut) { 'TIMEOUT' } elseif ($cleanupFailure) { 'CLEANUP_FAILURE' } elseif ($childExit -eq 0) { 'PASS' } else { 'FAIL' })
        $processRecords.Add($record)
    }
    if ($timedOut -or $cleanupFailure) { $script:exitCode = 3; throw "$Name timeout or owned-process cleanup failure" }
    return $record
}

try {
    if ($RunId -cnotmatch '^[A-Za-z0-9][A-Za-z0-9._-]{0,95}$' -or $RunId.EndsWith('.') -or $RunId -match '^(CON|PRN|AUX|NUL|COM[1-9]|LPT[1-9])(?:\.|$)') { throw 'Invalid RunId; expected a safe non-device identifier of at most 96 characters, without a trailing dot' }
    if (($Stage -eq 'Preflight' -and $Day -ne 41) -or ($Stage -eq 'Playtest' -and $Day -ne 59) -or ($Stage -in @('Soak','Finalize') -and $Day -ne 60)) { throw 'Stage does not apply to selected day' }
    $head = (& git -C $repoRoot rev-parse --verify HEAD).Trim()
    if ($LASTEXITCODE -ne 0 -or $head -cnotmatch '^[a-f0-9]{40}$') { throw 'Cannot resolve repository HEAD' }
    $dirtyPaths = @(& git -C $repoRoot status --porcelain=v1 --untracked-files=all)
    if ($LASTEXITCODE -ne 0) { throw 'Cannot inspect source state' }
    $runDir = Join-Path $repoRoot "Saved/Reports/GameplayExpansion/$head/$RunId/$Stage"
    Assert-NoReparse $runDir
    if (Test-Path -LiteralPath $runDir) { throw 'Evidence stage already exists; select a fresh RunId (no overwrite)' }
    $runParent = Split-Path $runDir -Parent
    [IO.Directory]::CreateDirectory($runParent) | Out-Null
    Assert-NoReparse $runParent
    # CreateNew is the exclusive claim: two callers can both observe an absent
    # directory, so Test-Path/New-Item alone cannot protect immutable evidence.
    $claimPath = Join-Path $runParent ('.' + $Stage + '.claim')
    $claim = [IO.File]::Open($claimPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
    try {
        Assert-NoReparse $runDir
        if (Test-Path -LiteralPath $runDir) { throw 'Evidence stage already exists; select a fresh RunId (no overwrite)' }
        New-Item -ItemType Directory -Path $runDir -ErrorAction Stop | Out-Null
    } finally { $claim.Dispose() }
    $resultPath = Join-Path $runDir ("day-{0}.json" -f $Day)
    if (-not $ScopePath) { $ScopePath = Join-Path $repoRoot 'Docs/Plans/Gameplay-Expansion-Implementation/gameplay-expansion-scope.json' }
    $result = [ordered]@{ schemaVersion=1; day=$Day; stage=$Stage; runId=$RunId; sourceRevision=$head; workingTreeDirty=($dirtyPaths.Count -gt 0); dirtyPaths=$dirtyPaths; gameplayProfile='GameplayExpansionV1'; startedAtUtc=$started.ToString('o'); status='BLOCKED'; reasonCode='IMPLEMENTATION_NOT_AVAILABLE'; runtimeEntry='BLOCKED_RUNTIME_ENTRY'; toolingStatus='NOT_RUN'; evidenceStatus='BLOCKED'; scopeHash=$null; contentHashes=@{}; packageManifestHash=$null; fixtureId='BaselineAudit'; seed=$null; topology=$Topology; composition=$Composition; expectedTestCount=$null; observedTestCount=$null; assertions=@(); captures=@(); traces=@(); childExits=@(); cleanup='NOT_RUN'; requestedPackageManifest=$PackageManifestPath }
    $result.inputHashes = Get-InputHashes $implementationPaths
    $result.contentHashes = Get-InputHashes $contentPaths
    $scopeHashAtStart = $(if (Test-Path -LiteralPath $ScopePath -PathType Leaf) { (Get-FileHash -LiteralPath $ScopePath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null })
    $result.scopeHash = $scopeHashAtStart
    $python = @(Get-Command python -CommandType Application -ErrorAction Stop)[0].Source
    $pwsh = @(Get-Command pwsh -CommandType Application -ErrorAction Stop)[0].Source
    if ($Day -in @(59,60) -and $Stage -eq 'Fast') {
        $testPath = Join-Path $runDir 'tooling-tests.json'
        $tests = Invoke-Owned 'tooling-tests' $python @((Join-Path $repoRoot 'Scripts/test_gameplay_expansion.py'), '--day', [string]$Day, '--output', $testPath) 600
        if ($tests.exitCode -ne 0 -or -not (Test-Path -LiteralPath $testPath)) { throw "Day $Day tooling tests failed or exported no report" }
        $toolingEvidence = Get-Content -LiteralPath $testPath -Raw | ConvertFrom-Json
        if ($toolingEvidence.schemaVersion -ne 1 -or $toolingEvidence.day -ne $Day -or $toolingEvidence.status -ne 'PASS' -or -not $toolingEvidence.passed -or $toolingEvidence.testsRun -le 0 -or @($toolingEvidence.failures).Count -ne 0) { throw "Day $Day tooling report is malformed or failed" }
        $result.toolingStatus = 'PASS'
        $result.toolingResultPath = $testPath
        $result.toolingResultSha256 = (Get-FileHash -LiteralPath $testPath -Algorithm SHA256).Hash.ToLowerInvariant()
        $result.toolingTestCount = $toolingEvidence.testsRun
        $result.reasonCode = $(if ($Day -eq 59) { 'FRESH_HUMAN_COHORT_REQUIRED' } else { 'FINAL_RUNTIME_EVIDENCE_REQUIRED' })
        $result.assertions = @('Synthetic tooling fixtures validate evidence handling only and cannot satisfy runtime, performance, author, or human gates.')
        $result.status = 'BLOCKED'; $result.runtimeEntry = 'BLOCKED_RUNTIME_ENTRY'; $result.evidenceStatus = 'BLOCKED'; $exitCode = 2
    } elseif ($Day -ne 41 -or $Stage -notin @('Preflight','Fast')) {
        $result.reasonCode = 'STAGE_IMPLEMENTATION_MISSING'
        $result.assertions = @('No gameplay/package/playtest/soak/finalizer execution is implemented by the Day41 adapter.')
    } else {
        $auditPath = Join-Path $runDir 'scope-audit.json'
        $auditArgs = @((Join-Path $repoRoot 'Scripts/gameplay_expansion.py'), 'audit', '--repo-root', $repoRoot, '--scope', $ScopePath, '--output', $auditPath)
        if ($BaselineEvidencePath) { $auditArgs += @('--baseline-evidence', $BaselineEvidencePath) }
        if ($EvidenceRoot) { $auditArgs += @('--evidence-root', $EvidenceRoot) }
        if ($PackageRoot) { $auditArgs += @('--package-root', $PackageRoot) }
        $auditProcess = Invoke-Owned 'scope-audit' $python $auditArgs 120
        if ($auditProcess.exitCode -eq 3) { $exitCode = 3 }
        if (-not (Test-Path -LiteralPath $auditPath)) { throw 'Validator produced no audit report' }
        $audit = Get-Content -LiteralPath $auditPath -Raw | ConvertFrom-Json
        if ($auditProcess.exitCode -eq 3) {
            if ($audit.schemaVersion -ne 1 -or $audit.command -ne 'audit' -or $audit.status -notin @('TIMEOUT','CLEANUP_FAILURE') -or $audit.evidenceStatus -ne $audit.status -or -not $audit.reasonCode) { throw 'Validator exit 3 lacks a valid timeout/cleanup report' }
            $auditProcess.status = $audit.status
            $exitCode = 3
            $result.reasonCode = $audit.reasonCode
            $result.evidenceStatus = $audit.evidenceStatus
            throw 'Scope/evidence validator reported a timeout or cleanup failure'
        }
        if ($auditProcess.exitCode -notin @(0,2)) { throw 'Scope/evidence validation failed; see scope-audit.json' }
        if ($audit.schemaVersion -ne 1 -or $audit.command -ne 'audit' -or $audit.status -notin @('PASS','BLOCKED') -or $audit.evidenceStatus -ne $audit.status -or $audit.runtimeEntry -notin @('READY','BLOCKED_RUNTIME_ENTRY') -or -not $audit.reasonCode -or -not @($audit.checks).Count) { throw 'Validator audit report is missing required fields or has inconsistent dispositions' }
        if (($auditProcess.exitCode -eq 0) -ne ($audit.status -eq 'PASS') -or ($audit.runtimeEntry -eq 'READY' -and $audit.status -ne 'PASS')) { throw 'Validator exit code and evidence/readiness dispositions disagree' }
        $auditProcess.status = $audit.status
        $result.scopeHash = $(if (Test-Path -LiteralPath $ScopePath -PathType Leaf) { (Get-FileHash -LiteralPath $ScopePath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null })
        $result.auditPath = $auditPath
        $result.evidenceStatus = $audit.evidenceStatus
        $result.runtimeEntry = $audit.runtimeEntry
        $result.reasonCode = $audit.reasonCode
        $result.assertions = $audit.checks
        # Capture observed machine descriptors; no trace/performance claim follows.
        $hardware = [ordered]@{ schemaVersion=1; kind='GameplayHardwareDiagnostic'; capturedAtUtc=[DateTime]::UtcNow.ToString('o'); benchmarkStatus='NOT_RUN'; displaySettings=$null; cpu=@(Get-CimInstance Win32_Processor | Select-Object Name,NumberOfCores,NumberOfLogicalProcessors); physicalMemoryBytes=(Get-CimInstance Win32_ComputerSystem).TotalPhysicalMemory; gpu=@(Get-CimInstance Win32_VideoController | Select-Object Name,DriverVersion,@{n='reportedAdapterRamBytes';e={$_.AdapterRAM}}); freeSpaceBytes=(Get-PSDrive -Name ([IO.Path]::GetPathRoot($repoRoot).Substring(0,1))).Free }
        Write-JsonFile (Join-Path $runDir 'hardware-diagnostic.json') $hardware
        $scopeValid = @($audit.checks | Where-Object { $_.name -eq 'ScopeRequiredFields' -and $_.status -eq 'PASS' }).Count -eq 1
        if ($Stage -eq 'Fast' -and $scopeValid) {
            $testPath = Join-Path $runDir 'tooling-tests.json'
            $tests = Invoke-Owned 'tooling-tests' $python @((Join-Path $repoRoot 'Scripts/test_gameplay_expansion.py'), '--day', '41', '--output', $testPath) 600
            if ($tests.exitCode -ne 0 -or -not (Test-Path -LiteralPath $testPath)) { throw 'Tooling tests failed or did not export a report' }
            $toolingEvidence = Get-Content -LiteralPath $testPath -Raw | ConvertFrom-Json
            if ($toolingEvidence.schemaVersion -ne 1 -or $toolingEvidence.day -ne 41 -or $toolingEvidence.status -ne 'PASS' -or $toolingEvidence.passed -isnot [bool] -or -not $toolingEvidence.passed -or $toolingEvidence.testsRun -isnot [long] -or $toolingEvidence.testsRun -le 0 -or $toolingEvidence.failures -isnot [array] -or $toolingEvidence.failures.Count -ne 0 -or $toolingEvidence.skipped -isnot [array] -or $toolingEvidence.skipped.Count -ge $toolingEvidence.testsRun) { throw 'Tooling report is malformed, failed or contains no executed passing tests' }
            $result.toolingTestsStatus = 'PASS'
            $result.toolingResultPath = $testPath
            $result.toolingResultSha256 = (Get-FileHash -LiteralPath $testPath -Algorithm SHA256).Hash.ToLowerInvariant()
            $result.toolingTestCount = $toolingEvidence.testsRun
            $result.toolingSkippedCount = $toolingEvidence.skipped.Count
            $result.toolingPassedCount = $toolingEvidence.testsRun - $toolingEvidence.skipped.Count
            $legacyRunId = 'gameplay-baseline-' + [Guid]::NewGuid().ToString('N')
            $legacy = Invoke-Owned 'legacy-fast' $pwsh @('-NoProfile','-NonInteractive','-File',(Join-Path $repoRoot 'RunPlayableCandidate.ps1'),'-Stage','Fast','-Mode','Both','-Role','Both','-RunId',$legacyRunId) 600
            $result.legacyRunId = $legacyRunId
            if ($legacy.exitCode -ne 0) { throw 'Existing local regression failed' }
            $legacyResultPath = Join-Path $repoRoot "Saved/Reports/PlayableCandidate/$head/$legacyRunId/fast-contract.json"
            if (-not (Test-Path -LiteralPath $legacyResultPath -PathType Leaf)) { throw 'Existing local regression exported no fresh fast-contract report' }
            $legacyEvidence = Get-Content -LiteralPath $legacyResultPath -Raw | ConvertFrom-Json
            if ($legacyEvidence.schemaVersion -ne 1 -or $legacyEvidence.days -ne 'all' -or $legacyEvidence.mode -ne 'Both' -or $legacyEvidence.passed -isnot [bool] -or -not $legacyEvidence.passed -or $legacyEvidence.testsRun -isnot [long] -or $legacyEvidence.testsRun -le 0 -or $legacyEvidence.failures -isnot [array] -or $legacyEvidence.failures.Count -ne 0) { throw 'Existing local regression report is malformed, filtered, failed or empty' }
            $result.legacyTestsStatus = 'PASS'
            $result.legacyFastResultPath = $legacyResultPath
            $result.legacyFastResultSha256 = (Get-FileHash -LiteralPath $legacyResultPath -Algorithm SHA256).Hash.ToLowerInvariant()
            $result.legacyTestCount = $legacyEvidence.testsRun
            if (-not $EngineRoot) {
                $candidates = @((Join-Path (Split-Path $repoRoot -Parent) 'UnrealEngine-5.5'))
                if ($env:UE_ROOT) { $candidates += $env:UE_ROOT }
                $found = @($candidates | Where-Object { Test-Path -LiteralPath (Join-Path $_ 'Engine/Build/Build.version') } | Select-Object -Unique)
                if ($found.Count -eq 1) { $EngineRoot = $found[0] }
            }
            if (-not $EngineRoot -or -not (Test-Path -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version'))) {
                $result.reasonCode = 'ENGINE_ROOT_REQUIRED'
            } else {
                $EngineRoot = (Resolve-Path -LiteralPath $EngineRoot).Path
                $version = Get-Content -LiteralPath (Join-Path $EngineRoot 'Engine/Build/Build.version') -Raw | ConvertFrom-Json
                if ($version.MajorVersion -ne 5 -or $version.MinorVersion -ne 5) { throw 'Expected Unreal Engine 5.5' }
                $result.engineRoot = $EngineRoot
                $result.engineVersion = '{0}.{1}.{2}' -f $version.MajorVersion,$version.MinorVersion,$version.PatchVersion
                $buildPath = Join-Path $EngineRoot 'Engine/Build/BatchFiles/Build.bat'
                $projectPath = Join-Path $repoRoot 'Aura.uproject'
                $buildCommand = "& '" + $buildPath.Replace("'","''") + "' AuraEditor Win64 Development '" + $projectPath.Replace("'","''") + "' -WaitMutex -NoHotReloadFromIDE`nexit `$LASTEXITCODE"
                $build = Invoke-Owned 'editor-build' $pwsh @('-NoProfile','-NonInteractive','-Command',$buildCommand) 1800
                if ($build.exitCode -ne 0) { throw 'AuraEditor build failed' }
                $nativeRoot = Join-Path $runDir 'legacy-native'
                $nativeLog = Join-Path $runDir 'legacy-native.log'
                $nativeStart = [DateTime]::UtcNow.ToString('o')
                $editor = Join-Path $EngineRoot 'Engine/Binaries/Win64/UnrealEditor-Cmd.exe'
                # This headless regression run does not collect a performance
                # trace. Do not auto-launch a persistent service outside its job.
                # The separate rendered baseline still requires actual tracing.
                $native = Invoke-Owned 'native-process' $editor @($projectPath,'-unattended','-nop4','-NullRHI','-NoSound','-NoSplash','-NoLiveCoding','-notraceserver','-ExecCmds=Automation RunTests Aura.RoleBattle','-TestExit=Automation Test Queue Empty',"-ReportExportPath=$nativeRoot","-abslog=$nativeLog") 600
                $nativeResult = Join-Path $runDir 'native-result.json'
                $parse = Invoke-Owned 'native-parse' $python @((Join-Path $repoRoot 'Scripts/gameplay_expansion.py'),'native','--index',(Join-Path $nativeRoot 'index.json'),'--log',$nativeLog,'--exit-code',[string]$native.exitCode,'--started-at',$nativeStart,'--report-root',$nativeRoot,'--output',$nativeResult) 120
                if ($parse.exitCode -eq 3) {
                    $exitCode = 3
                    if (-not (Test-Path -LiteralPath $nativeResult -PathType Leaf)) { throw 'Native parser exit 3 lacks a timeout/cleanup report' }
                    $timedNative = Get-Content -LiteralPath $nativeResult -Raw | ConvertFrom-Json
                    if ($timedNative.schemaVersion -ne 1 -or $timedNative.command -ne 'native' -or $timedNative.status -notin @('TIMEOUT','CLEANUP_FAILURE') -or $timedNative.evidenceStatus -ne $timedNative.status -or -not $timedNative.reasonCode) { throw 'Native parser exit 3 has an invalid timeout/cleanup report' }
                    $parse.status = $timedNative.status
                    $exitCode = 3
                    $result.reasonCode = $timedNative.reasonCode
                    $result.nativeEvidenceStatus = $timedNative.status
                    throw 'Native parser reported a timeout or cleanup failure'
                }
                if ($native.exitCode -ne 0 -or $parse.exitCode -notin @(0,2) -or -not (Test-Path -LiteralPath $nativeResult)) { throw 'Native baseline failed validation' }
                $nativeEvidence = Get-Content -LiteralPath $nativeResult -Raw | ConvertFrom-Json
                if ($nativeEvidence.schemaVersion -ne 1 -or $nativeEvidence.command -ne 'native' -or $nativeEvidence.status -notin @('PASS','BLOCKED') -or (($parse.exitCode -eq 0) -ne ($nativeEvidence.status -eq 'PASS')) -or $nativeEvidence.evidenceStatus -ne $nativeEvidence.status -or $nativeEvidence.nativeTestsStatus -ne 'PASS' -or $nativeEvidence.expectedTestCount -isnot [long] -or $nativeEvidence.expectedTestCount -le 0 -or $nativeEvidence.observedTestCount -isnot [long] -or $nativeEvidence.observedTestCount -ne $nativeEvidence.expectedTestCount -or -not $nativeEvidence.reasonCode -or -not @($nativeEvidence.checks).Count) { throw 'Native baseline report is malformed, empty or disagrees with its parser exit' }
                $parse.status = $nativeEvidence.status
                $result.nativeResultPath = $nativeResult
                $result.nativeResultSha256 = (Get-FileHash -LiteralPath $nativeResult -Algorithm SHA256).Hash.ToLowerInvariant()
                $result.nativeTestsStatus = $nativeEvidence.nativeTestsStatus
                $result.expectedTestCount = $nativeEvidence.expectedTestCount
                $result.observedTestCount = $nativeEvidence.observedTestCount
                $result.toolingStatus = 'PASS'
                if ($nativeEvidence.status -eq 'BLOCKED') {
                    $result.runtimeEntry = 'BLOCKED_RUNTIME_ENTRY'
                    $result.reasonCode = $nativeEvidence.reasonCode
                    $result.nativeEvidenceStatus = 'BLOCKED'
                    $result.nativeWarningTestCount = $nativeEvidence.warningTestCount
                } else { $result.nativeEvidenceStatus = 'PASS' }
            }
        }
        if ($Stage -eq 'Fast' -and $result.toolingStatus -ne 'PASS') { $result.runtimeEntry = 'BLOCKED_RUNTIME_ENTRY' }
        if ($result.runtimeEntry -eq 'READY') { $result.status = 'PASS'; $exitCode = 0 }
        else { $result.status = 'BLOCKED'; $exitCode = 2 }
        Write-JsonFile (Join-Path $runDir 'prerequisite-remediation.json') ([ordered]@{ runtimeEntry=$result.runtimeEntry; reasonCode=$result.reasonCode; missing=@($audit.checks | Where-Object { $_.status -in @('BLOCKED','FAIL') }); nextProducer='Complete the explicit Day40 packaged/journey/soak/Shipping operations and measured Day41 baseline contracts; implement unsupported evidence readers before freeze.' })
    }
} catch {
    if ($exitCode -ne 3) { $exitCode = 1 }
    if ($result) {
        $result.status = $(if ($exitCode -eq 3) { 'TIMEOUT_OR_CLEANUP_FAILURE' } else { 'FAIL' })
        $result.runtimeEntry = 'BLOCKED_RUNTIME_ENTRY'
        if ($exitCode -ne 3) { $result.reasonCode = 'RUNNER_FAILURE' }
        elseif ($result.evidenceStatus -notin @('TIMEOUT','CLEANUP_FAILURE') -and $result.nativeEvidenceStatus -notin @('TIMEOUT','CLEANUP_FAILURE')) { $result.reasonCode = 'PROCESS_TIMEOUT_OR_CLEANUP_FAILURE' }
        $result.error=$_.Exception.Message
    }
    Write-Host $_.Exception.Message
} finally {
    if ($result -and $resultPath) {
        try {
            $result.inputHashesAtEnd = Get-InputHashes $implementationPaths
            $result.contentHashesAtEnd = Get-InputHashes $contentPaths
            $changedInputs = @($implementationPaths | Where-Object { $result.inputHashes[$_] -ne $result.inputHashesAtEnd[$_] })
            $changedInputs += @($contentPaths | Where-Object { $result.contentHashes[$_] -ne $result.contentHashesAtEnd[$_] })
            $headAtEnd = (& git -C $repoRoot rev-parse --verify HEAD).Trim()
            if ($LASTEXITCODE -ne 0 -or $headAtEnd -cnotmatch '^[a-f0-9]{40}$') { throw 'Cannot resolve repository HEAD at completion' }
            $result.sourceRevisionAtEnd = $headAtEnd
            if ($headAtEnd -ne $head) { $changedInputs += 'sourceRevision' }
            $scopeHashAtEnd = $(if (Test-Path -LiteralPath $ScopePath -PathType Leaf) { (Get-FileHash -LiteralPath $ScopePath -Algorithm SHA256).Hash.ToLowerInvariant() } else { $null })
            if ($scopeHashAtStart -ne $scopeHashAtEnd) { $changedInputs += 'requestedScope' }
            $result.changedInputs = @($changedInputs)
            $result.inputsUnchanged = $changedInputs.Count -eq 0
            if ($changedInputs.Count -gt 0) {
                $result.runtimeEntry = 'BLOCKED_RUNTIME_ENTRY'
                if ($exitCode -ne 3) { $exitCode = 1; $result.status = 'FAIL'; $result.reasonCode = 'SOURCE_CHANGED_DURING_RUN' }
            }
        } catch {
            $result.inputsUnchanged = $false
            $result.inputHashError = $_.Exception.Message
            $result.runtimeEntry = 'BLOCKED_RUNTIME_ENTRY'
            if ($exitCode -ne 3) { $exitCode = 1; $result.status = 'FAIL'; $result.reasonCode = 'INPUT_HASH_CAPTURE_FAILED' }
        }
        $result.completedAtUtc = [DateTime]::UtcNow.ToString('o')
        $result.durationSeconds = $clock.Elapsed.TotalSeconds
        $result.childExits = @($processRecords.ToArray())
        $result.cleanup = $(if (@($processRecords | Where-Object { $_.cleanup -ne 'PASS' }).Count) { 'FAIL' } else { 'PASS' })
        $result.exitCode = $exitCode
        Write-JsonFile $resultPath $result
        Write-Host ("{0}: {1}; runtime entry {2}; report {3}" -f $Stage,$result.status,$result.runtimeEntry,$resultPath)
    }
}
exit $exitCode
