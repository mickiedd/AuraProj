# Day 58 — Gameplay Performance

Status: Evidence schema, budgets, fail-closed validator, and native contract tests implemented. Full sign-off remains BLOCKED until real rendered baseline/candidate traces meet the frozen budgets.
Depends on: Day 57 passing staged technical inventory; Day 41 recorded reference hardware/settings; Day 55 full gameplay matrix. Day 57's unavailable second-author row remains an open final usability gate, not a substitute for or blocker to technical profiling.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Keep busy gameplay responsive and stable using measured changes at the actual bottlenecks.

## Exact change surfaces

- Existing/planned: encounter coordinator, enemy AI target queries, attack timeline/status/cue actors, mission snapshots and native WebUI view model.
- Existing: `Source/Aura/Private/Character/AuraEnemy.cpp`, `Private/Player/AuraPlayerState.cpp`, selected replicated actors/components and UE5.5 profiling/Insights support.
- New: `Content/Config/GameplayPerformanceBudgets.json`, `Scripts/RunGameplayPerformance.ps1`, `Docs/Reports/Gameplay-Performance-<revision>-<run-id>.md`; adapter emits trace/metric summaries, not source-pattern PASS.

## Data and authority contract

Freeze matched hardware/build/scene settings before comparison. Day 41 supplies the reference machine/settings and any honestly recorded legacy baseline; it cannot supply a measurement of future gameplay content. Capture the actual Day 58 pre-optimization package/content/scene and raw metrics before changing performance code, then compare against the optimized candidate using matched hardware, occupancy, inputs and settings. Dedicated30Hz: game-thread p95<=25ms, p99<=33.3ms; gameplay scheduling p95<=2ms. Client1080p Medium p95<=22ms, p99<=33.3ms. Outbound per client p95<=100KiB/s and <=25% growth against that matched pre-optimization actor-count baseline. Five-minute warmup +10-minute sample, three repeats. Ten-cycle hub return restores mission actor/timer/effect counts and <=5% memory growth. NullRHI/headless data cannot satisfy rendered client budget.

## Numbered implementation steps

1. Record CPU/GPU/RAM/driver, engine/build, resolution, scalability, server tick, network conditions and separate baseline/candidate package/content hashes. Capture the pre-optimization evidence before changes at low/mid/max occupancy including boss+four adds within caps; retain it as comparison provenance, never relabel it as a final-candidate measurement.
2. Use Unreal Insights timing/memory/network traces and per-frame samples; compute percentiles from raw data with declared warmup exclusion. Report hitch causes and trace overhead separately.
3. Optimize the largest measured cost first: stagger 5Hz target searches, reuse immutable definitions, avoid global actor scans, coalesce changed snapshots, reduce irrelevant cosmetic replication, bound cue actors.
4. Preserve damage/collision/telegraph timing and owner privacy. Tune replication frequency/relevance only after a late-relevance/reconnect test; do not blindly disable necessary state.
5. Run matched before/after samples and seeded gameplay parity tests. Reject an optimization that hides enemies, delays tells, changes objective order or loses replicated updates.
6. Execute ten run/abort/settle cycles and compare object/delegate/timer/effect baselines; produce explicit PASS/FAIL/BLOCKED for each budget and required machine.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay58Tests.cpp`; namespace `Aura.Gameplay.Day58`.

- SchedulingBudgetMeasured — trace samples meet declared p95 under maximum supported occupancy.
- ReplicationParityAfterOptimization — late relevance and reconnect reconstruct the same state.
- NoGameplaySemanticDrift — accepted damage/objective/reward outcomes match fixed input trace.
- BoundedActorAndTimerCounts — hub baseline restored after ten cycles.
- NoProgressiveMemoryGrowth — measured post-warmup ten-cycle growth <=5%.
- MetricEvidenceRequired — absent traces, hardware data or insufficient sample duration cannot pass.
- BaselineAndCandidateIdentityDistinct — comparison retains the actual pre-optimization identity and matched settings; acceptance metrics bind the final candidate, and a Day 41 legacy sample cannot masquerade as the future gameplay baseline.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 58 -Stage Fast -RunId d58-fast
./RunGameplayExpansion.ps1 -Day 58 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d58-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

PerfEnvelope: solo10 enemies, paired16 enemies, boss+four adds, eight simultaneous cues, escort+hazard+HUD choices; all three templates on D-AB and L-AB plus rendered S-A/S-B. Three matched samples each; 150ms/2%loss regression overlay.

## Failure and timeout semantics

Trace capture has 20-minute per-sample timeout, process limits remain shared. Missing reference hardware is BLOCKED; a budget miss is FAIL needing optimization or explicit scope/hardware revision before retest. No silent threshold loosening after measurement. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-58.json, performance.json, raw-frame-times.csv, bandwidth.csv, .utrace captures, hardware/settings hashes and matched before-after analysis. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All frozen budgets meet measured evidence without authority, cue, state or objective regressions; cleanup counts are exact and memory remains bounded.

## Defer / anti-goals

No Mass/ECS conversion, Iris/ReplicationGraph migration, custom transport, speculative pooling, unmeasured multithreading or optimization justified only by code style.
