# Shared execution contract — Days 41–60

Status: Day 41 tooling implementation started; later gameplay stages remain planned. This file is normative for every linked daily plan. The Day 41 Preflight/Fast tooling increment is implemented as recorded in the current review report. Later gameplay runner stages, new native groups, runtime fixtures and full evidence readers remain **implementation deliverables**, not commands claimed to work today. Existing candidate runner semantics remain unchanged.

## Read order and daily gate

Read the [roadmap](../Gameplay-Expansion-Implementation-Plan-2026-08-31.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md), this contract, then the selected day. A dependent day consumes its predecessors' artifacts and tests. A failed predecessor cannot be hidden with a skipped row. Day 41 can finish its planning subset with a BLOCKED runtime-entry result; Day 42 cannot begin runtime changes until Day 40 evidence meets its inherited requirements and Day 41 is Frozen/READY. Shipping identity and second-developer operations checks required by the old contract cannot be waived by labeling them external.

Each implementation day includes: content/config validation, actual engine tests of behavior, relevant old regressions, a visual walkthrough, bounded failure paths, a machine result and the project-required dated SVG/Markdown/archive-index record. Tests named in a daily plan are planned test cases with explicit observable assertions; Python source scanning is not a substitute for native/runtime behavior.

## New outputs and ownership

- Day 41: `Docs/Plans/Gameplay-Expansion-Implementation/gameplay-expansion-scope.json`, recording schema/version, old candidate artifact/hash, source-at-freeze, assumptions, profile, feature inventory, fixture matrix, performance hardware and budgets. It has explicit Draft and Frozen phases. Draft stores unavailable evidence and source-at-freeze as null with typed blockers. Frozen requires verified predecessor provenance, measured baseline/hardware and a surveyed mission space; Draft never authorizes Day 42.
- Day 41: `Scripts/ValidateGameplayExpansionScope.ps1`, `Scripts/gameplay_expansion.py`, `Scripts/test_gameplay_expansion.py`, `Scripts/GameplayExpansionProcess.cs` and `RunGameplayExpansion.ps1` adapter. The adapter validates scope and reports unsupported stages/readers as BLOCKED; it never writes placeholder PASS records.
- Day 42: `Scripts/RunGameplayExpansionDay.ps1` plus actual engine launch/result parsing for selected fixtures. Reuse the project's candidate/RoleBattle process ownership patterns; do not copy its local-only checks and call them packaged tests. Factor a shared launcher only when necessary, retaining old runner compatibility tests.
- Native implementation tests (Days 42–59 where runtime behavior is under test): `Source/Aura/Private/Tests/AuraGameplayDayNNTests.cpp`, test namespace `Aura.Gameplay.DayNN`. Shared network probe planned at `Source/Aura/Public/Tests/AuraGameplayNetworkProbe.h` and `Private/Tests/AuraGameplayNetworkProbe.cpp`; diagnostic mutation entry points are Development-only, not Shipping gameplay RPCs.
- Definition schemas begin with the owner day; Day 57 integrates their staged manifest at `Content/Config/GameplayExpansionManifest.json`. Do not wait until Day 57 to validate new definitions.

## Command contract

Run the new adapter from repository root in PowerShell 7; its process API requires version 7. The following syntax is the frozen interface to implement, not an invocation performed during planning:

The adapter's `-Stage` accepts `Preflight`, `Fast`, `Packaged`, `Playtest`, `Soak`, and `Finalize`. Stage/day applicability is explicit: Preflight on Day 41, Playtest on Day 59, Soak and Finalize on Day 60. Unsupported combinations are argument errors. `-SeedSet Core` uses seed 41001 for ordinary daily functional lanes and all three seeds for RNG/assembly assertions; Days 55/60 expand the exact matrix described below. Performance uses the separately named Day 58 runner.

```powershell
python Scripts/test_gameplay_expansion.py --day 44 --output Saved/Reports/GameplayExpansion/day44-static.json
./RunGameplayExpansion.ps1 -Day 44 -Stage Fast -RunId d44-fast
./RunGameplayExpansion.ps1 -Day 44 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d44-packaged -PackageManifestPath <explicit-package-manifest>
```

Replace 44 with the selected day; each day declares its native cases and fixture. Day 41 is the explicit exception: Preflight validates evidence only; Fast runs Python tooling tests, existing local regressions and the real `Aura.RoleBattle` native baseline, not a nonexistent `Aura.Gameplay.Day41` namespace. Both Day 41 stages return exit 2 while runtime entry remains blocked, with separate local-test outcomes. Its Packaged stage is explicitly BLOCKED until a public-input baseline driver is implemented. Day 59 separates native telemetry/privacy/modified-gameplay cases from Python playtest aggregation cases. Day 60 finalizer cases are Python behavioral tests; its Fast stage runs those plus the existing Aura.Gameplay native suite from Days 42–59 and legacy regressions, without inventing an Aura.Gameplay.Day60 namespace. `Fast` builds AuraEditor Win64 Development, validates definitions, invokes `UnrealEditor-Cmd.exe` with `-unattended -NullRHI -NoSound -NoLiveCoding -ExecCmds="Automation RunTests Aura.Gameplay.Day44" -TestExit="Automation Test Queue Empty" -ReportExportPath=<run-dir>`, and parses exported per-test results. Use an explicitly discovered UE 5.5 engine path, not a fabricated installation path. NullRHI tests cannot satisfy rendered cues/visual usability.

The adapter accepts `-EngineRoot <UE5.5-root>` when discovery is ambiguous and records the resolved path/version. Build via that engine's `Engine/Build/BatchFiles/Build.bat AuraEditor Win64 Development <absolute Aura.uproject> -WaitMutex -NoHotReloadFromIDE`; propagate failures. Game/Server package creation reuses existing project packaging, produces an explicit manifest with SHA-256 for each executable and staged content, and precedes `Packaged`.

`Packaged` uses the actual packaged server/client/host binaries from `-PackageManifestPath`, performs public input-driven scenarios, and captures server/client logs. Probe observations corroborate the public flow; a fixture that directly writes wallet/health/objective success is diagnostic only. Test harness setup can seed a controlled fixture on authority before play. Missing binaries, renderer, identity preflight, fixtures or expected reports yield BLOCKED, not PASS. Never select a latest package/result automatically.

Existing regressions that are available now: `./RunPlayableCandidate.ps1 -Stage Fast -Mode Both -Role Both`, `python Scripts/test_playable_candidate.py --days all`, and native `Aura.RoleBattle`. Their local result is only a regression subset. Dedicated/listen proof requires packaged execution. The new profile uses independent scope/result directories and must not rewrite the old scope or candidate.json.

## Fixtures and topology matrix

All gameplay fixtures use `GameplayExpansionV1`, StartupMap, level ID `RoleBattleCivilianTest`. `Core` seeds are 41001, 41002, 41003; fault seed 41999 injects only the declared failure. Fixtures are IDs in the new scope manifest; verified world transforms are added by Day 43, not guessed in scripts.

| Lane | Processes | Composition |
| --- | --- | --- |
| S-A / S-B | One rendered packaged standalone game | Aura / BungeeMan |
| L-AA / L-BB / L-AB / L-BA | Packaged listen host + one remote client | Same-role pairs; mixed pair with both host-role orientations |
| D-AA / D-BB / D-AB | Packaged dedicated server + two remote clients | Same-role pairs and mixed pair |

Default daily functional matrix is all nine lanes on seed 41001 for the day's scenario. Tests with no applicable feature still prove no regression/NotApplicable behavior; Aura never acquires dummy ammo. Day 55/60 replay matrix adds all three templates × two layouts × Standard on all nine lanes (54 runs, seed 41001), then both mutators for each template/layout on D-AB (12 runs, seed 41002); Core seeds also replay the deterministic assembly test. Pairwise augment coverage is generated from valid role eligibility, not all mathematically possible pairs.

Network failure overlay: D-AB and L-AB at measured/simulated 150ms RTT and 2% packet loss. Record actual directional lag settings and observed RTT; do not confuse 150ms one-way with 150ms RTT. Reconnect the same participant at 10s and at 61s; new late entrant waits. Run forced server kill and graceful shutdown during active combat and settlement. Existing legacy four lanes remain separate regression rows.

Use new per-run fixture persistence IDs and existing allowlisted development identity mechanism. Never reuse real player saves or count `OnlineSubsystemNull`/fixture identities as production authentication. The scope declares the profile namespace and exact isolated directories; cleanup deletes only resolved run-owned paths. No global process kill or deletion outside the declared run root. Start helpers hidden; preserve existing user sessions.

## Timeouts and result schema

Budgets: build 30 minutes; package 45 minutes; native suite 10 minutes; process boot 120 seconds; readiness 60 seconds after boot; scenario 20 minutes plus two-minute teardown/settlement allowance; process graceful shutdown 15 seconds then terminate only owned child PIDs and collect exit state. Day 60 ten-cycle soak per topology has a 240-minute cap. Runners emit progress at least every 15 seconds and expose cancellation; an agent invoking them must yield/poll rather than block its own communications for minutes.

`0 = PASS` for the requested stage, `1 = FAIL`, `2 = BLOCKED or DIAGNOSTIC` with typed status, `3 = TIMEOUT/CLEANUP_FAILURE`. A filtered stage can pass its requested diagnostic work but cannot set overall candidate PASS. Missing/empty test discovery, report parse error, child exit mismatch, missing mandatory lane, orphan process or malformed input is nonzero. Stage-local PASS is not day/candidate completion.

Every result goes under `Saved/Reports/GameplayExpansion/<SourceRevision>/<RunId>/`. Daily files are `day-NN.json`; additional evidence names are specified per day. Required fields: schemaVersion, stage, day, scopeHash, sourceRevision, workingTreeDirty, gameplayProfile, runId, contentHashes, packageManifestHash, fixtureId, seed, topology/composition, expected/observed test counts, assertions, status, reasonCode, start/end UTC, monotonic duration, child exits, logs/captures/traces and cleanup result. Privacy follows the old run-scoped HMAC convention; no raw stable IDs or credentials.

Finalizer on Day 60 accepts explicit `-TechnicalPath`, `-PlaytestPath`, `-PerformancePath`, `-SoakPath`, `-RegressionPath`, `-PackageManifestPath`, `-Revision` and `-RunId`; binds all gameplay records to the exact final source/scope/content/package. Legacy-regression evidence records the same binary package under its own unchanged legacy scope. The old Day 40 prerequisite may be from an ancestor commit; prove ancestry and record its hash, then rerun regressions on final binaries. It is not required to pretend Day 40 used future source. Finalization requires clean committed source and writes `gameplay-candidate.json` once; later fixes require a new RunId and revalidation.

Specialized stage commands to implement (placeholders require explicit files, never a latest-directory search):

```powershell
./Scripts/RunGameplayPerformance.ps1 -PackageManifestPath <package-manifest> -BudgetPath Content/Config/GameplayPerformanceBudgets.json -RunId d58-perf -OutputPath <performance-json>
./RunGameplayExpansion.ps1 -Day 59 -Stage Playtest -SessionManifestPath <consented-session-manifest> -PackageManifestPath <package-manifest> -RunId d59-playtest
./RunGameplayExpansion.ps1 -Day 60 -Stage Soak -Topology All -CyclesPerTopology 10 -PackageManifestPath <package-manifest> -RunId final-gameplay-run
./RunGameplayExpansion.ps1 -Day 60 -Stage Finalize -Revision <final-commit> -RunId final-gameplay-run -TechnicalPath <technical-json> -PlaytestPath <playtest-json> -PerformancePath <performance-json> -SoakPath <soak-json> -RegressionPath <regression-json> -PackageManifestPath <package-manifest>
```

The Playtest stage validates/imports consented observations; it does not recruit participants or synthesize answers. The final RunId can group explicitly supplied stage records through declared stageRunIds; each must match source/scope/content/package while retaining its original stage RunId. Mixing unmatched acceptance content versions is forbidden. Day 58's separately identified pre-optimization comparison baseline retains its own source/content/package hashes as provenance only; it is never candidate acceptance evidence. Its fixture, hardware, actors and settings must match the final comparison protocol, and the post-optimization acceptance samples bind to the final package. The finalizer records hashes of all supplied records, and all emitted file names are scoped to their stage to avoid overwriting a prior `day-NN.json`.

## Human evaluation and review

Daily captures show the actual rendered game where visuals matter; browser DOM snapshots alone cannot prove in-engine cue/HUD timing. The Day 59 protocol includes consent, pseudonymous participants and opt-out from video; no user recruitment/messages are sent without authorization.

For **runtime code implementation**, follow the standing in-app ChatGPT independent-review workflow before completion, including action-time transmission rules and no duplicate sends. If unavailable, record the limitation and use local validation; never claim external review occurred. The current roadmap-writing task modifies documentation/SVG only and uses a local planning audit, not an external code review.
