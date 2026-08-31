# Day 55 — Replay Assembly and Mutators

Status: Planned; not implemented by this planning job.  
Depends on: Day 54 complete loop; Days 47/49/50 builds, layouts and pacing.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Vary repeat runs through explicit compatible combinations while preserving reproducibility and fairness.

## Exact change surfaces

- New: `Source/Aura/Public/Gameplay/AuraMissionAssemblyPolicy.h` and implementation; `Content/Config/GameplayMutatorDefinitions.json`, `GameplayAssemblyRules.json`.
- Extend mission, encounter, arena and augment registries; run snapshots and preparation UI.
- Extend `Scripts/test_gameplay_expansion.py`, gameplay fixture manifest and network runner matrix; no parallel level generator.

## Data and authority contract

Three templates × two validated arrangements × Standard or one of two opt-in mutators. Restless Patrols adds one finite Raider/Lancer patrol recipe per cell while retaining live/pressure caps. Volatile Vents enables extra prevalidated hazard pads with full tells, never blocking mandatory routes. No combined mutators. No reward multiplier in this milestone; optional challenge is clearly labeled before both players accept. Dedicated RNG streams for assembly, encounters and offers isolate consumption; record algorithm version, seed, content hash and stream counters.

## Numbered implementation steps

1. Define explicit compatibility table for each template/layout/mutator; reject impossible escort/hazard intersections, undersized spawn anchors and missing required interactions.
2. Implement seed-derived recipe ordering and separate streams; normal server selects seed, test builds accept an allowlisted seed for reproduction. Clients cannot mutate seed after readiness.
3. Keep minimum required roster/objective/reward stable across assembly; randomization changes order/placement within validated envelopes rather than hiding difficulty spikes.
4. Add preparation preview with mission verb, layout thumbnail, mutator danger and no bonus promise. Both players confirm changed selection; timeout cancels preparation.
5. Run all 54 standard matrix combinations and 12 mutator D-AB combinations defined in the shared contract; replay Core seeds and corrupt references deliberately.
6. Conduct a second combined checkpoint: the exploratory participants play two different builds/templates and explain what changed. Record this as a scheduled comparison, not a voluntary-replay or fresh first-use acceptance result. Remove combinations that feel identical or unfair before final polish; reserve Day 59's fresh cohort for the frozen candidate.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay55Tests.cpp`; namespace `Aura.Gameplay.Day55`.

- SeparateRandomStreams — extra visual/random calls do not alter enemy or offer sequence.
- SeedDecisionReplay — same algorithm/content/inputs reproduces assembly and admission decisions.
- CompatibilityRejectsBlockedEscort — dangerous pad/nav combination fails preflight.
- MutatorCapsAndCounters — patrol/vent modifiers cannot break live caps, cue lead or role viability.
- PreparationBothPlayersConsent — host cannot force a changed modifier after remote ready.
- MatrixCompleteness — missing/duplicate template-layout-lane row is nonzero.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 55 -Stage Fast -RunId d55-fast
./RunGameplayExpansion.ps1 -Day 55 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d55-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

ReplayMatrix: 54 Standard rows on seed41001, 12 mutator D-AB rows on41002, algorithm repeat checks on41001–41003; gameplay observations use comparable builds and fresh run IDs.

## Failure and timeout semantics

Invalid assembly blocks before loadout entry; three bounded candidate recipes then AssemblyUnavailable, not random retries until one looks successful. Definition hot-reload is disabled during active runs. A content change invalidates reproduction hashes and requires new evidence. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-55.json, assembly-compatibility-matrix.json, rng-stream-traces.json, matrix-lane-results.json and replay-checkpoint-observations.md. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

Every supported combination is playable and reproducible at the decision level; player observations distinguish the variations; no role, objective, cue or reward invariant is broken.

## Defer / anti-goals

No procedural geometry, infinite seeds advertised as unique levels, stacked mutators, randomized loot tiers, leaderboard seed competition or daily live-service rotation.
