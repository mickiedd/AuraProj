# Day 13 - Add Civilian Population Lifecycle

## Goal

Extend the existing Day 09 `UAuraPopulationManager` with Civilian corpse cleanup and deterministic slot refill. The first slice deliberately leaves Enemy creation, loot, lifespan, destruction, and respawn in the existing `MonsterSpawnTable`/`AAuraGameModeBase` path so there is never a second Enemy respawn owner.

## BungeeMan Gun Skill checkpoint

Verify that a permitted FireGun death of a Civilian marks exactly one canonical population slot, removes the corpse once, and schedules at most one refill. A projectile impact or late replicated callback must not create a second population transition.

## Prerequisite gate

- Day 09 initial population and deterministic `PopulationId:SlotIndex` registry pass in listen and dedicated modes.
- Day 11 records exactly one attributed Civilian death against the existing manager and marks the slot dead without scheduling a refill.
- Day 12 exposes authority-only phase/zone queries and server phase-change delegates.

## New files

- `RunRoleBattleDay13NetworkSmoke.ps1`

## Files to modify

- `Source/Aura/Public/World/AuraPopulationTypes.h`
- `Source/Aura/Public/World/AuraPopulationManager.h`
- `Source/Aura/Private/World/AuraPopulationManager.cpp`
- `Source/Aura/Public/Character/AuraCivilian.h`
- `Source/Aura/Private/Character/AuraCivilian.cpp`
- `Source/Aura/Public/Game/AuraGameModeBase.h`
- `Source/Aura/Private/Game/AuraGameModeBase.cpp`
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`
- `Content/Config/PopulationSpawnTable.json`
- `Content/Maps/RoleBattleCivilianTest.umap`

Do not modify `AAuraEnemy`, `MonsterSpawnTable.json`, `SpawnedMonsterRows`, or `OnSpawnedMonsterDestroyed` on Day 13 except to run regression tests. Existing Enemy respawn remains solely owned by `AAuraGameModeBase`.

## Scope and ownership

`UAuraPopulationManager` remains the one owner of Civilian population rows, slots, actor mappings, corpse timers, refill timers, marker reservations, and debug snapshots.

It does not:

- Register Enemy actors.
- Listen to Enemy death events for refill.
- Schedule Enemy timers.
- mirror `SpawnedMonsterRows`.
- replace or wrap the existing Monster spawn table.

Future unification of Enemy and Civilian population systems requires a separate migration that first removes the GameMode Enemy callbacks. It is not part of this vertical slice.

## Civilian member-slot state

Extend each deterministic slot with:

- `Empty`: no actor and eligible for initial spawn/refill.
- `Active`: one living registered Civilian.
- `Dying`: exactly-once death event received.
- `Corpse`: dead actor retained for presentation.
- `RefillPending`: corpse removed/hidden and one refill timer exists.
- `Suppressed`: policy/phase prevents refill and no timer is active.

Every state transition records population ID, slot index, member ID, generation, actor weak pointer, authoritative server time, and reason.

Only the Day 11 dispatcher's neutral death event may move Active→Dying. The matching actor's authoritative Dying→Dead life-state callback moves Dying→Corpse and starts the corpse timer. `OnDestroyed` is cleanup/diagnostic input and must not infer a second death or schedule a second refill.

## Exactly-once death, corpse, and count contract

When `RecordAuthoritativeDeath` receives a Civilian event:

1. Resolve the slot from the canonical member ID and verify the event actor matches the registered actor.
2. Reject an unknown member, actor mismatch, stale manager generation, non-Civilian death policy, duplicate death sequence, or slot not Active.
3. Atomically move Active→Dying, record `DeathSequence`, and decrement active count once.
4. Release work/observation/shelter reservations and all AI/interaction registry entries.
5. Observe the matching authority life-state transition. Dying→Dead moves the slot Dying→Corpse exactly once and starts one tracked corpse timer using the validated row policy. If the actor is already Dead when the neutral event arrives, perform that transition immediately; a zero corpse duration still records Corpse before cleanup.
6. The corpse timer may fire only from Corpse. It revalidates generation/member/actor/death sequence/state, destroys or hides the corpse once, clears the weak actor, then moves to RefillPending or Suppressed according to policy.

Repeated death events, repeated actor destruction, and OnRep callbacks cannot alter counts or create timers.

## Refill contract

- Refill always targets the same vacant slot and reuses the exact `PopulationId:SlotIndex` member ID.
- Active plus reserved/pending slots can never exceed `maximumCount`.
- Every timer is stored in a map keyed by canonical member ID; attempting to add a second timer is rejected and logged.
- Timer delegates capture a weak manager/owner reference, manager generation, population ID, and slot index rather than a raw actor pointer.
- Refill uses the Day 09 deferred-spawn, role validation, nav/collision validation, and commit path.
- A failed spawn releases the reservation and schedules at most one bounded retry according to policy. It cannot recursively create timers.
- Before scheduling and again when executing, query the Day 12 director for current phase, zone enabled state, and row allowed phases.
- A phase change to a disallowed state cancels active refill timers and marks affected slots Suppressed.
- A later allowed phase re-evaluates Suppressed slots and schedules at most one refill each.
- A refill never occurs in a safe/suppressed zone merely because it was scheduled during a prior Conflict phase.

The population row’s `respawnPolicy.enabled=false` leaves the slot Suppressed after cleanup.

## Shutdown and world-transition contract

`UAuraPopulationManager::Shutdown` is idempotent and is called from `AAuraGameModeBase::EndPlay` before manager references are released.

Shutdown:

- Marks the manager generation inactive.
- Unbinds once from `UAuraDeathPolicyDispatcher::OnAuthoritativeDeath`, actor life-state callbacks, and Day 12 phase delegates.
- Cancels every corpse, refill, retry, and reservation timeout handle.
- Clears marker reservations and actor delegates.
- Prevents queued callbacks from spawning into a tearing-down world.
- Clears the registry only after callbacks are invalidated.

`EndPlay` reasons caused by level transition/server shutdown do not schedule replacements. A fresh world rebuilds the deterministic initial population through Day 09.

## Population snapshot

Add a server-only immutable debug snapshot containing:

- Schema/config version and manager generation.
- Population/map/zone/work IDs.
- Per-slot state and canonical member ID.
- Actor network name/path when active/corpse.
- Active, corpse, pending, suppressed, and maximum counts.
- Remaining corpse/refill delay.
- Last transition reason and death sequence.
- Current director phase/event ID.

This is the source shape Day 18 may persist; Day 13 does not save it.

## Implementation steps

1. Extend the Day 09 slot state and manager; do not create another manager.
2. Consume only validated Civilian death events and update counts once.
3. Track corpse cleanup by member ID and generation.
4. Add deterministic same-slot refill with bounded spawn retry.
5. Bind phase changes and revalidate policy both when scheduling and executing.
6. Cancel/resume timers without duplicates as phase policy changes.
7. Add idempotent manager shutdown and world-teardown safeguards.
8. Add the immutable debug snapshot.
9. Leave every existing Enemy spawn/respawn callback untouched and add isolation regression coverage.
10. Add the native and network tests below.

## Native automation

Add:

- `Aura.RoleBattle.Day13.CivilianDeathDecrementsOnce`
- `Aura.RoleBattle.Day13.CorpseCleanupOnce`
- `Aura.RoleBattle.Day13.FullSlotTransitionSequence`
- `Aura.RoleBattle.Day13.StableSlotRefill`
- `Aura.RoleBattle.Day13.MaximumNeverExceeded`
- `Aura.RoleBattle.Day13.NoDuplicateTimers`
- `Aura.RoleBattle.Day13.PhaseChangeCancelsAndResumes`
- `Aura.RoleBattle.Day13.ExecutionRevalidatesPhase`
- `Aura.RoleBattle.Day13.ShutdownCancelsCallbacks`
- `Aura.RoleBattle.Day13.EnemyRespawnIsolation`

The Enemy isolation test must kill an existing table-spawned Enemy and prove exactly one existing GameMode respawn occurs while the Civilian manager’s registry/timers remain unchanged.

## Listen-server and dedicated-server smoke

`RunRoleBattleDay13NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`. Each mode:

1. Kill one population Civilian with exactly-once attributed damage.
2. Verify active count decrements once, corpse cleanup occurs once, and only one refill timer exists.
3. Verify the replacement uses the same member ID/slot and active count never exceeds maximum.
4. Schedule a refill, move the director to a disallowed phase before expiry, and prove no spawn occurs.
5. Return to an allowed phase and prove exactly one replacement occurs.
6. Kill several civilians simultaneously and verify independent deterministic slots and no duplicate tasks.
7. Trigger level teardown with pending corpse/refill timers and prove no post-teardown spawn/log error.
8. Kill one existing Enemy and prove only the legacy GameMode path respawns it.
9. Connect a late client after cleanup/refill and compare replicated population identities and actor count.

The runner enforces bounded startup, corpse/refill/retry, assertion, late-join, and teardown timeouts; returns nonzero on any assertion, child-process, crash, timeout, duplicate timer/spawn, or missing-artifact failure; and stops only processes it created. It writes `Saved/Logs/Day13-{Listen|Dedicated}-{Server|Client1|Client2}.log` and `Saved/Reports/Day13-{Listen|Dedicated}.json` with revision, commands, exit codes, slot transitions, and assertion results.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day13; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day13Automation.log'

& '.\RunRoleBattleDay13NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay13NetworkSmoke.ps1' -Mode Dedicated
```

## Completion gate

The Day 09 manager owns the complete Civilian-only first-slice lifecycle: exactly-once death accounting, corpse cleanup, phase-aware deterministic same-slot refill, bounded retries, snapshots, and shutdown cancellation. It never registers or respawns an Enemy, and the existing Enemy GameMode path still produces exactly one respawn. All build, native, listen, and dedicated gates pass.
