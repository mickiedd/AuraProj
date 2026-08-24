# Day 10 - Add Civilian AI

## Goal

Implement one server-only Unreal Engine Behavior Tree for the first Civilian loop: idle, wander, work, observe, flee, and shelter. Do not use BehaviorU for Civilian AI and do not create a second life/death state; AI interruption reads the replicated combat life state introduced on Day 03.

## BungeeMan Gun Skill checkpoint

Keep FireGun a player-owned ability; Civilian AI must never receive or activate it. Flee/threat behavior may react only to an authoritative, rule-permitted FireGun hit and must stop on `Dying`/`Dead`; client target data or a raw gun tag cannot drive AI state.

## Prerequisite gate

- Day 03 exposes Alive/Dying/Dead through `UAuraCombatStateComponent` and `AuraCombatRules`.
- Day 09 owns the Civilian registry and supplies validated `workProfileId`, `zoneId`, and deterministic member identity.
- Three Day 09 civilians spawn on nav-valid locations in listen and dedicated modes.

## Chosen runtime

Day 10 uses Unreal `AAIController`, `UBehaviorTree`, `UBlackboardData`, native services/tasks/decorators, `NavigationSystem`, and the existing `AIModule` dependency.

Do not add `UBehaviorUAgentComponent` to `AAuraCivilian`, do not load a BehaviorU XML graph, and do not run an AI state machine on clients. BehaviorU remains an unrelated existing Enemy/test integration.

## New code files

- `Source/Aura/Public/AI/AuraCivilianAIController.h`
- `Source/Aura/Private/AI/AuraCivilianAIController.cpp`
- `Source/Aura/Public/AI/AuraCivilianBehaviorTypes.h`
- `Source/Aura/Public/AI/AuraCivilianWorkProfileRegistry.h`
- `Source/Aura/Private/AI/AuraCivilianWorkProfileRegistry.cpp`
- `Source/Aura/Public/AI/BTService_FindNearestThreat.h`
- `Source/Aura/Private/AI/BTService_FindNearestThreat.cpp`
- `Source/Aura/Public/AI/BTService_FindNearestHostile.h`
- `Source/Aura/Private/AI/BTService_FindNearestHostile.cpp`
- `Source/Aura/Public/AI/BTService_UpdateCivilianContext.h`
- `Source/Aura/Private/AI/BTService_UpdateCivilianContext.cpp`
- `Source/Aura/Public/AI/BTTask_FindCivilianDestination.h`
- `Source/Aura/Private/AI/BTTask_FindCivilianDestination.cpp`
- `Source/Aura/Public/AI/BTTask_SetCivilianActivity.h`
- `Source/Aura/Private/AI/BTTask_SetCivilianActivity.cpp`
- `Source/Aura/Public/AI/BTDecorator_CivilianAlive.h`
- `Source/Aura/Private/AI/BTDecorator_CivilianAlive.cpp`
- `Source/Aura/Public/World/AuraCivilianActivityMarker.h`
- `Source/Aura/Private/World/AuraCivilianActivityMarker.cpp`
- `Source/Aura/Public/World/AuraCivilianWorkMarker.h`
- `Source/Aura/Private/World/AuraCivilianWorkMarker.cpp`
- `Source/Aura/Public/World/AuraCivilianObservationMarker.h`
- `Source/Aura/Private/World/AuraCivilianObservationMarker.cpp`
- `Source/Aura/Public/World/AuraCivilianShelterMarker.h`
- `Source/Aura/Private/World/AuraCivilianShelterMarker.cpp`
- `RunRoleBattleDay10NetworkSmoke.ps1`

## New content assets

- `Content/AI/Civilian/BB_Civilian.uasset`
- `Content/AI/Civilian/BT_Civilian.uasset`
- `Content/Blueprints/World/Civilian/BP_CivilianWorkMarker.uasset`
- `Content/Blueprints/World/Civilian/BP_CivilianObservationMarker.uasset`
- `Content/Blueprints/World/Civilian/BP_CivilianShelterMarker.uasset`
- `Content/Blueprints/AI/Services/BTS_FindNearestHostile.uasset`
- `Content/Blueprints/AI/Services/BTS_FindNearestHostile.snapshot.json`

## Files and assets to modify

- `Source/Aura/Public/Character/AuraCivilian.h`
- `Source/Aura/Private/Character/AuraCivilian.cpp`
- `Source/Aura/Public/AI/BTService_FindNearestPlayer.h`
- `Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp`
- `Source/Aura/Public/World/AuraPopulationManager.h`
- `Source/Aura/Private/World/AuraPopulationManager.cpp`
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`
- `Content/Blueprints/Character/Civilian/BP_AuraCivilian.uasset`
- `Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree.uasset`
- `Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree.snapshot.json`
- `Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree_Elementalist.uasset`
- `Content/Blueprints/AI/BehaviorTree/BT_EnemyBehaviorTree_Elementalist.snapshot.json`
- `Content/Blueprints/AI/Services/BTS_FindNearestPlayer.uasset`
- `Content/Blueprints/AI/Services/BTS_FindNearestPlayer.snapshot.json`
- `Content/Config/CivilianWorkProfiles.json`
- `Content/Maps/RoleBattleCivilianTest.umap`

After both Enemy Behavior Tree assets have been migrated and repository search finds no remaining class/asset reference, delete:

- `Source/Aura/Public/AI/BTService_FindNearestPlayer.h`
- `Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp`
- `Content/Blueprints/AI/Services/BTS_FindNearestPlayer.uasset`
- `Content/Blueprints/AI/Services/BTS_FindNearestPlayer.snapshot.json`

Do not leave a deprecated class redirect or duplicate service ticking beside the replacement unless an external cooked asset is proven to require a one-release redirect.

## Behavior and blackboard contract

`EAuraCivilianActivity` contains only:

- Idle
- Wander
- Work
- Observe
- Flee
- Shelter

It does not contain Dying or Dead. `UAuraCombatStateComponent` remains the single source for life state. Any state other than Alive aborts the Behavior Tree and clears movement.

`BB_Civilian` defines these exact keys:

- `ThreatActor`: Object, Actor base class.
- `ThreatDistance`: Float, default maximum float.
- `Destination`: Vector.
- `Activity`: Enum `EAuraCivilianActivity`.
- `bHasSafeDestination`: Bool.
- `HomeLocation`: Vector.
- `WorkMarker`: Object.
- `ObservationMarker`: Object.
- `ShelterMarker`: Object.
- `LastMoveFailureTime`: Float.

`BT_Civilian` uses this priority:

1. A root `BTDecorator_CivilianAlive` aborts the tree when life state leaves Alive.
2. If `ThreatActor` is valid, select an eligible shelter first; if none exists, select a nav-reachable flee point away from the threat. Move with a bounded timeout and reselect after failure.
3. If safe after a configurable calm period, clear the threat and resume the schedule.
4. Choose a work marker matching `workProfileId` and zone, otherwise an observation marker, otherwise bounded wander.
5. Use built-in Move To/Wait nodes plus the native destination/activity tasks. No attack task or ability activation node exists.

## Activity marker contract

Create markers on Day 10 rather than relying on the future battle-zone implementation:

- Every marker has a unique `MarkerId`, `ZoneId`, enabled flag, capacity, acceptance radius, and gameplay-tag requirements.
- `AAuraCivilianWorkMarker` adds allowed work-profile IDs.
- `AAuraCivilianObservationMarker` adds facing/observation duration.
- `AAuraCivilianShelterMarker` adds protection radius and capacity.
- Markers register idempotently with the Day 09 population manager on authority. Clients receive normal actor replication only for visualization; they do not reserve marker capacity.
- Reservations are keyed by deterministic population member ID and are released on state change, failed move, Dying/Dead, actor destruction, or manager shutdown.
- Day 12 may associate these marker IDs with a battle zone, but Day 10 can select them using their own `ZoneId` now.

The test map must contain at least one work marker, one observation marker, and two shelter markers, including one intentionally unreachable shelter fixture.

## Work-profile runtime contract

`UAuraCivilianWorkProfileRegistry` reads the versioned `CivilianWorkProfiles.json` already introduced on Day 09. It exposes immutable validated profiles containing:

- Work/wander durations.
- Wander radius.
- Threat radius and calm duration.
- Flee distance and move timeout.
- Allowed work/observation/shelter marker tags.
- Walk and flee speeds.

The GameMode-owned population manager owns the registry. Missing or invalid profiles prevent that population row from spawning; AI does not silently use arbitrary defaults.

## Threat-query contract

`UBTService_FindNearestThreat` runs on the server only and answers “which nearby candidate may currently damage this Civilian?”

For each perceived/query candidate:

1. Reject null, self, missing/invalid combat identity, or non-Alive candidates. Do not require the potential source to be targetable; source targetability does not determine whether it can inflict damage.
2. Call `AuraCombatRules::CanDamage(Candidate, Civilian, ServerResolvedContext)` directly in that direction.
3. Reject Friendly, Neutral, Protected, and any denied result.
4. Apply radius and line-of-sight checks from the work profile.
5. Select deterministically by threat score, distance, then actor network GUID/name as a stable tie-breaker.

Never call `CanCombatTarget(Civilian, Candidate)`: Civilian correctly has `bCanAttack=false`. Before Day 12 exists, production `ServerResolvedContext` uses the Day 03 default relationship policy, so a protected Civilian has no eligible damage threat. Day 10 automation and network smokes exercise the flee branch through Day 03's `WITH_DEV_AUTOMATION_TESTS` authority-only trusted policy fixture; clients and production runtime cannot install it. Day 12 later replaces only context resolution, not the service contract.

The service must use a bounded registry/perception candidate set. Do not copy the existing per-tick `GetAllActorsOfClass` scan for every Civilian.

## Enemy FindNearestHostile migration

Day 03 deliberately retained the temporary class name `UBTService_FindNearestPlayer`. Day 10 completes that migration:

- Add `UBTService_FindNearestHostile` as the single hostile-AI acquisition service.
- Preserve the existing editable selectors and blackboard contract exactly: `TargetToFollow` remains an Object key and `DistanceToTarget` remains a Float key.
- Preserve the no-target result: null `TargetToFollow` and `TNumericLimits<float>::Max()` distance.
- Resolve the owning hostile pawn, enumerate a bounded target registry/perception set, and call `AuraCombatRules::CanCombatTarget(Hostile, Candidate, ServerResolvedContext)`. Also require candidate Alive, targetable, reachable, and permitted by the current damage/relationship policy.
- Select deterministically by threat/priority, distance, then stable actor tie-breaker.
- Keep the service authority-only and retain rate-limited diagnostics using the new `FindNearestHostile` name.
- Replace the service node class in `BT_EnemyBehaviorTree.uasset` and `BT_EnemyBehaviorTree_Elementalist.uasset` without renaming either blackboard key or changing downstream Move/Attack tasks.
- Create `BTS_FindNearestHostile` parented to the new native class, preserve the old Blueprint service's editable selector values, replace both tree-node references with the new generated class, and refresh its snapshot.
- Refresh both snapshot JSON files and verify they no longer serialize `BTService_FindNearestPlayer`.
- Remove the old header/source and `BTS_FindNearestPlayer` asset/snapshot only after an Asset Registry/reference scan plus C++/config/snapshot search is clean.

This migration is an Enemy regression task, not a reason to reuse hostile attack branches in the Civilian tree.

## Server-only execution and interruption

- `AAuraCivilian` sets `AIControllerClass=AAuraCivilianAIController` and `AutoPossessAI=PlacedInWorldOrSpawned`.
- Only authority calls `SpawnDefaultController`. AIController and BrainComponent never exist as decision owners on clients.
- `AAuraCivilianAIController::OnPossess` validates the pawn, blackboard, Behavior Tree, work profile, and combat-state component before running.
- Bind to the Day 03 life-state delegate. Dying/Dead calls `StopLogic`, `StopMovement`, clears focus/marker reservations, and cannot restart.
- Client-observed movement comes from CharacterMovement replication. Activity may replicate as read-only debug/UI state, but clients cannot write it.
- A failed move times out, releases the reservation, records `LastMoveFailureTime`, and selects a different candidate. This prevents permanent stuck loops.

## Implementation steps

1. Add the exact controller, blackboard, tree, services, tasks, decorator, and marker classes/assets.
2. Add immutable work-profile loading and validation.
3. Assign the controller/tree to the Civilian Blueprint and enforce authority-only startup.
4. Register and reserve activity markers through the existing population manager.
5. Implement the inverse directional `CanDamage(Candidate, Civilian)` threat query.
6. Add `UBTService_FindNearestHostile` and `BTS_FindNearestHostile`, migrate both Enemy Behavior Tree assets while preserving their blackboard contract, and retire the old native and Blueprint `FindNearestPlayer` services.
7. Implement bounded work/observe/wander and shelter/flee selection with failure recovery.
8. Stop and clean up AI on the Day 03 Dying/Dead transition.
9. Add rate-limited activity, threat, marker reservation, path failure, recovery, and Enemy acquisition logs.
10. Add the native and network tests below.

## Native automation

Add:

- `Aura.RoleBattle.Day10.WorkProfileValidation`
- `Aura.RoleBattle.Day10.ThreatQueryDirection`
- `Aura.RoleBattle.Day10.ProtectedCandidateIgnored`
- `Aura.RoleBattle.Day10.TrustedTestThreatPolicy`
- `Aura.RoleBattle.Day10.UntargetableDamageSourceStillThreat`
- `Aura.RoleBattle.Day10.MarkerReservationCapacity`
- `Aura.RoleBattle.Day10.UnreachableShelterRecovery`
- `Aura.RoleBattle.Day10.LifeStateStopsBrain`
- `Aura.RoleBattle.Day10.NoAttackNodeOrGrant`
- `Aura.RoleBattle.Day10.FindNearestHostileBlackboardContract`
- `Aura.RoleBattle.Day10.EnemyTargetingRegression`
- `Aura.RoleBattle.Day10.NoFindNearestPlayerReferences`

## Current executable baseline smoke

`RunRoleBattleDay10NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`. The 2026-08-24 baseline passes both modes and records eight assertions: static contracts, coordinated world readiness, a server probe proving three stable members with three authority-owned running Behavior Trees plus registered markers and the migrated hostile asset, director replication on both clients, all three population states on both clients including late join, and no crash signature.

This baseline is executable integration evidence, not the full Day 10 completion gate.

## Remaining completion acceptance matrix

Before Day 10 is marked complete, extend the runner or a successor to:

1. Spawn at least three civilians through Day 09 and verify only the server owns their AIControllers.
2. Observe deterministic work/observe/wander transitions without any ability activation.
3. Spawn a relationship-denied candidate and prove it is ignored.
4. Install the Day 03 authority-only trusted test policy, spawn an allowed Enemy threat, and prove directional `CanDamage(Enemy, Civilian)` selects it even when the source itself is non-targetable; prove the remote client cannot install or forge that policy.
5. Verify civilians reserve distinct available shelters, recover from the unreachable fixture, and flee when capacity is exhausted.
6. Remove the threat and verify they return to schedule after the calm period.
7. Transition one Civilian to Dying through the Day 03 test hook and verify movement, BrainComponent, and marker reservation stop once.
8. Verify each Enemy Behavior Tree uses `FindNearestHostile`, still writes `TargetToFollow`/`DistanceToTarget`, selects an allowed Player, ignores protected/friendly candidates, and continues its existing Move/Attack behavior.
9. Search loaded classes, the Asset Registry, both Enemy trees, the Services directory, snapshots, source, and config for `BTService_FindNearestPlayer`/`BTS_FindNearestPlayer`; fail if any old service instance ticks or reference remains.
10. Compare replicated transforms/activity debug state on two clients and a late joiner.

The current baseline runner enforces bounded startup, assertion, late-join, and teardown timeouts; returns nonzero on any implemented assertion, child-process, crash, timeout, or missing artifact; and stops only processes it created. The completion extension must retain those guarantees while adding the navigation/action matrix above. Reports are written to `Saved/Reports/Day10-{Listen|Dedicated}.json` beside isolated process logs.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day10; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day10Automation.log'

& '.\RunRoleBattleDay10NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay10NetworkSmoke.ps1' -Mode Dedicated
```

## Completion gate

At least three population-managed civilians run the one chosen Unreal Behavior Tree exclusively on authority, work/observe/wander without attacking, identify eligible threats through directional `CanDamage(Candidate, Civilian)`, exercise flee through the authority-only non-shipping policy fixture, reserve valid shelters, recover from failed paths, and stop immediately when the Day 03 life state leaves Alive. Enemy Behavior Trees use only the new native/Blueprint `FindNearestHostile` service while preserving `TargetToFollow`/`DistanceToTarget` and existing combat behavior; no native or asset `FindNearestPlayer` reference remains. All build, native, listen, and dedicated gates pass.
