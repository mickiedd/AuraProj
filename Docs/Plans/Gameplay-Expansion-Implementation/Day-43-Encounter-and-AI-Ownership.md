# Day 43 — Encounter and AI Ownership

Status: Planned; not implemented by this planning job.  
Depends on: Day 42 lifecycle/isolation; Day 41 verified anchor survey.  
Normative context: [shared execution contract](Execution-Contract.md), [architecture](../Gameplay-Expansion-Architecture-2026-08-31.md).

## Player outcome

Replace the temporary objective with two ordered finite hostile encounters that spawn once, behave coherently, and clean up completely.

## Exact change surfaces

- Existing: `Source/Aura/Private/Actor/AuraEnemySpawnPoint.cpp`, `AuraEnemySpawnVolume.cpp`, `Private/Character/AuraEnemy.cpp`, `Private/AI/AuraBehaviorUAgentComponent.cpp`, `Private/AI/AuraAIController.cpp`.
- New: `Source/Aura/Public/Gameplay/AuraEncounterCoordinator.h`, `AuraEncounterTypes.h`, `AuraEnemyArchetypeComponent.h` with private implementations.
- New: `Content/Config/GameplayEncounterDefinitions.json`, `GameplayEnemyArchetypes.json`, `GameplayArenaLayouts.json`; existing UE BT tasks under `Source/Aura/Private/AI/` and explicit validated combat-tree asset references.
- Extend Day 42 mission types/state/subsystem with current-cell identity and exactly-once CellCompleted events; keep this simple two-cell sequence available before Day 47 consumes its boundaries.

## Data and authority contract

The coordinator owns (RunId, EncounterId, SlotId, Generation) leases, pending/live/corpse counters and accepted death membership. The mission owner orders two Clear cells of three enemies each, publishes current-cell identity and emits CellCompleted once per cell generation; only the second completion unlocks extraction. Day 47 consumes these existing boundaries and Day 48 later generalizes their objective data. Mission enemies use only the regular UE combat BT; disable/unregister BehaviorU's test movement before it ticks. Legacy non-mission fixtures retain their selected behavior. The Civilian population manager does not own hostiles. New profile suppresses legacy loot/XP; accepted enemy death only advances the objective here.

## Numbered implementation steps

1. Add authority, class/world validity and deferred-spawn null guards to legacy entry points; adapt the gameplay path to return typed spawn results instead of silently dereferencing a failed actor.
2. Write surveyed arrangement A transforms into the layout definition. Validate nav projection, capsule fit, hub exclusion and paired-client visibility checks; no arbitrary off-map fallback.
3. Prepare then commit a lease only after actor, controller, combat tree, class data and identity initialize. Release failed leases; cap live hostiles at 10 solo/16 paired including boss.
4. Select exactly one AI driver before BeginPlay/possession starts movement. Instrument driver activation counts and movement ownership; retain old BehaviorU test coverage.
5. Replace Day 42 stub with two ordered three-enemy Clear cells. The mission subsystem owns current-cell identity and exactly-once CellCompleted notifications; complete cell one before admitting cell two and require both before extraction. Count only registered authoritative deaths in the active cell. A Destroy callback releases capacity but cannot count as a kill; foreign/future-cell events cannot advance progression.
6. Cancel timers/BT tasks and release all leases on clear, failure, disconnect abort or world teardown. Add stale-generation and repeated-overlap stress cases.

## Named tests and commands

Native file: `Source/Aura/Private/Tests/AuraGameplayDay43Tests.cpp`; namespace `Aura.Gameplay.Day43`.

- ClientSpawnRejected — client overlap/spawn invocation creates zero authoritative or local gameplay enemies.
- FailedDeferredSpawnReleasesLease — null class/spawn failure returns typed failure with pending/live counts unchanged.
- ExactlyOneAIDriver — mission enemy records one active movement/attack driver on listen and dedicated.
- FiniteRosterClear — duplicate death/destroy/corpse cleanup increments Clear only once per accepted death.
- OrderedCellBoundaryOnce — two three-enemy cells emit one CellCompleted each in order; cell-one completion cannot unlock extraction or future-cell events.
- StaleGenerationCannotRefill — a prior encounter timer cannot populate the next run.
- HubCleanupNoGrowth — ten start/abort cycles restore actor, timer and delegate counts.

Planned runner commands (implement the adapter before use):

```powershell
./RunGameplayExpansion.ps1 -Day 43 -Stage Fast -RunId d43-fast
./RunGameplayExpansion.ps1 -Day 43 -Stage Packaged -Topology All -Composition All -SeedSet Core -RunId d43-packaged -PackageManifestPath <explicit-package-manifest>
```

Fast includes native result discovery and content validation. Packaged requires actual game processes and rendered evidence; static checks alone cannot complete this day. Execute relevant existing `Aura.RoleBattle` regressions and the shared command contract.

## Fixtures and topologies

EncounterOwnership: two sequential cells of three basic Warrior enemies; three invalid anchors; one failed class; simultaneous overlaps from both clients; destroy one live actor without damage; duplicate/stale/future-cell completion events; old BT/BehaviorU fixtures as regressions.

## Failure and timeout semantics

Three spawn anchors at one-second intervals, then SpawnUnavailable abort; an impossible required spawn never grants completion. Technical destroy gets one bounded replacement attempt under a new generation, then abort. No infinite respawn, unbounded retry or crowd growth. Process timeouts, isolation, child cleanup and nonzero propagation follow the shared execution contract.

## Artifacts and evidence

day-43.json, lease-ledger.json, cell-boundary-events.json, ai-driver-trace.json, layout-nav-report.json, invalid-spawn logs and cleanup counts; before/after enemy behavior clip. Store evidence under the revision/RunId directory; bind scope/content/package hashes and distinguish technical assertions from human observations. Add the dated SVG, Markdown archive record and project memory-index entry required by AGENTS.md.

## Completion gate

All nine lanes clear both finite cell rosters in order with exactly one boundary event each, exactly one AI driver owns mission behavior, authority/null cases fail safely and ten cycles leave no live leases/timers.

## Defer / anti-goals

No population-manager generalization, BehaviorU rewrite, entity pooling, large crowds, or new navigation technology.
