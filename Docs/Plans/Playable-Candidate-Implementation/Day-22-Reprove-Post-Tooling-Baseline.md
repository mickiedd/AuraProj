# Day 22 — Re-prove the Post-Tooling Baseline

Status: Planned  
Depends on: Day 21 contract

## Goal

Establish a fresh, trustworthy baseline after the smoke-supervisor, result-composition, persistence-isolation, and XML-discovery changes in revision `6e78692`.

## Work

- Run the relevant Editor, Game, and Server Development/Shipping builds.
- Run focused/full native automation, Python contracts, AuraAbilityGraph smoke, AuraAutoTest, staged-data checks, and the complete long listen/dedicated/package sweep.
- Use the existing bounded supervisors and isolated child-process wrappers; do not bypass them with ad hoc launches.
- Triage only failures reproduced at the candidate revision; classify old evidence separately from new regressions.

## Detailed execution contract

### Files to inspect or modify

- **Build:** `build_test.bat`, `BuildDedicatedServer.bat`, `build.ps1`, `Source/Aura.Target.cs`, `Source/AuraServer.Target.cs`, and enabled plugin build files.
- **Harness:** `RunReviewSmokeSuite2.ps1`, `RunSmokeTestSupervisor.ps1`, `RunRoleBattleDay18PersistenceSmoke.ps1`, `RunRoleBattleDay19Multiplayer.ps1`, graph smoke, and AuraAutoTest entry points.
- **New output:** `Saved/Reports/PlayableCandidate/<SourceRevision>/<RunId>/day-22-baseline.json`; do not overwrite historical Day 20 evidence.

### Detailed steps

1. Capture revision, engine/platform, binary paths, run ID, timeout policy, and clean/dirty state before launching any process.
2. Build the required Editor, Game, and Server Development/Shipping targets from a fresh invocation; record stdout/stderr, exit code, and produced binary timestamp. Every build/test/package child has a declared timeout (120 seconds for an individual smoke stage unless the command manifest declares a larger bounded value), and the aggregate cannot convert a timeout into success.
3. Run the focused Role/Battle namespace, full `Aura` namespace, Python contracts, AuraAbilityGraph smoke, AuraAutoTest, and staged-content checks; record discovered counts and failures.
4. Assert exhaustive discovery and staging of every `Content/AbilityDefinitions/*.xml` file, including case-sensitive relative paths and role references.
5. Execute existing persistence and multiplayer listen/dedicated probes with new per-run log paths, ports, client identities, and `WorldPersistenceId` values. The baseline must use the Day 21 lane topology definitions; a filtered diagnostic run is not a replacement for the four-lane baseline.
6. At teardown, wait for every owned child, verify port release and no orphan Unreal process, then close every report handle.
7. Inject one malformed-content or failed-prerequisite case and prove the aggregate returns nonzero without replacing the successful report.
8. Classify reproduced failures as harness, product, content, packaging, or environment and assign a reproduction command before Day 23.

### Required automation and gate

- Every selected command and child process must exit successfully; a printed “passed” line without a matching exit/result record is insufficient.
- No fixed-log reuse, shared persistence namespace, orphan after 30 seconds, false-success aggregate, missing artifact, or unexplained harness failure is allowed.
- The baseline report is the only accepted input revision for Day 23 feature work.

## Validation and evidence

- Record every command, exit code, process lifetime, child-process cleanup result, log path, report path, and persistence namespace.
- Confirm no fixed-log reuse, orphaned Unreal process, false-success aggregate, or shared `WorldPersistenceId`.
- Re-run the packaged XML manifest assertion over every `Content/AbilityDefinitions/*.xml` file.

## Deep-review closure

- **Owner surfaces:** existing `build.ps1`, `RunReviewSmokeSuite2.ps1`, `RunSmokeTestSupervisor.ps1`, `RunRoleBattleDay18PersistenceSmoke.ps1`, and `RunRoleBattleDay19Multiplayer.ps1`; Day 22 wraps these rather than replacing them.
- **Required artifacts:** `Saved/Reports/PlayableCandidate/<SourceRevision>/<RunId>/day-22-baseline.json`, command/exit-code table, per-process cleanup record, staged XML manifest report, and a list of reproduced failures.
- **Gate:** every selected command exits zero, every owned child is gone within 30 seconds of teardown, every run has a unique `WorldPersistenceId` and log path, and the packaged XML audit is exhaustive. Any timeout, orphan, reused log, false-success aggregate, or unexplained harness failure blocks Day 23.

## Completion gate

The candidate has a fresh post-tooling baseline with zero unexplained harness failures. Any product regression is assigned before Day 23 begins.

## Defer

Do not add player features while a post-tooling baseline failure could invalidate their verification.
