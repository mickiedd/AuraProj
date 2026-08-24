# Day 11 - Complete the Death Lifecycle

## Goal

Extend the minimal replicated `UAuraCombatStateComponent` introduced on Day 03 into one authoritative, idempotent death pipeline. Exactly one caller may win Alive→Dying; only that winner may dispatch the policy selected by `DeathPolicyTag`, create rewards, publish a death event, or schedule cleanup/respawn.

Day 11 must remove reward and respawn decisions from inheritance-driven `Die` overrides. The one GameMode-owned dispatcher publishes a server-wide neutral death event that the existing Day 09 population manager consumes now and the Day 12 battle director can subscribe to later without a direct Day 11 dependency.

## BungeeMan Gun Skill checkpoint

Carry FireGun's source avatar, ability tag, and damage type into the fatal-damage context. A lethal BungeeMan projectile must enter the shared `Alive -> Dying` path once, dispatch exactly one role-appropriate policy, and preserve late-join death presentation without a projectile-side respawn or reward shortcut.

## Prerequisite gate

- Day 03 combat rules and the replicated combat-state component expose Alive/Dying/Dead and reject non-Alive damage targets.
- Day 04 damage context contains authoritative source, controller/player state, ability, and damage-type attribution.
- Day 09 `UAuraPopulationManager` exists and can resolve a Civilian by canonical population member ID.
- Day 10 AI stops from the Day 03 life-state delegate.

## New files

- `Source/Aura/Public/Combat/AuraDeathPolicyDispatcher.h`
- `Source/Aura/Private/Combat/AuraDeathPolicyDispatcher.cpp`
- `RunRoleBattleDay11NetworkSmoke.ps1`

## Files to modify

- `Source/Aura/Public/Combat/AuraCombatTypes.h`
- `Source/Aura/Public/Combat/AuraCombatStateComponent.h`
- `Source/Aura/Private/Combat/AuraCombatStateComponent.cpp`
- `Source/Aura/Public/Combat/AuraCombatRules.h`
- `Source/Aura/Private/Combat/AuraCombatRules.cpp`
- `Source/Aura/Public/Interaction/CombatInterface.h`
- `Source/Aura/Public/Character/AuraCharacterBase.h`
- `Source/Aura/Private/Character/AuraCharacterBase.cpp`
- `Source/Aura/Public/Character/AuraCharacter.h`
- `Source/Aura/Private/Character/AuraCharacter.cpp`
- `Source/Aura/Public/Character/AuraEnemy.h`
- `Source/Aura/Private/Character/AuraEnemy.cpp`
- `Source/Aura/Public/Character/AuraCivilian.h`
- `Source/Aura/Private/Character/AuraCivilian.cpp`
- `Source/Aura/Public/AbilitySystem/AuraAttributeSet.h`
- `Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp`
- `Source/Aura/Public/AuraAbilityTypes.h`
- `Source/Aura/Private/AuraAbilityTypes.cpp`
- `Source/Aura/Public/AI/AuraCivilianAIController.h`
- `Source/Aura/Private/AI/AuraCivilianAIController.cpp`
- `Source/Aura/Public/World/AuraPopulationManager.h`
- `Source/Aura/Private/World/AuraPopulationManager.cpp`
- `Source/Aura/Public/Game/AuraGameModeBase.h`
- `Source/Aura/Private/Game/AuraGameModeBase.cpp`
- `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`
- `Content/Maps/RoleBattleCivilianTest.umap`

The AttributeSet and ability-context paths above are the repository’s actual paths; do not create obsolete `AbilitySystem/Attributes` or `Public/AbilitySystem/AuraAbilityTypes.h` directories.

## Combat-state contract

Day 03 remains the owner of the base life-state enum. Day 11 extends the component with:

- `bool TryEnterDying(const FAuraFatalDamageContext& Context, FAuraDeathEvent& OutEvent)`.
- Authority-only Dying→Dead and Dead→Respawning transition functions.
- A replicated monotonically increasing `DeathSequence`.
- `OnRep_LifeState` and `OnRep_DeathSequence` reconciliation.
- Idempotent common-presentation helpers that are safe when called by a multicast, an OnRep, or late-join initialization.

`TryEnterDying`:

1. Rejects non-authority, invalid context, or any current state other than Alive.
2. Changes Alive→Dying and increments `DeathSequence` on the game thread.
3. Builds one immutable `FAuraDeathEvent`.
4. Returns `true` only to the caller that performed the transition.

Every rejected caller returns `false` and is forbidden from dispatching policy, rewards, loot, delegates, timers, or additional presentation.

`Respawning` is used only for the existing player-respawn handoff. Population refill creates a new Civilian actor in the same deterministic slot; the dead Civilian actor does not transition to Respawning.

## Fatal-damage and attribution structures

Add `FAuraFatalDamageContext` and `FAuraDeathEvent` to `AuraCombatTypes.h`.

The fatal context contains server-resolved:

- Victim actor, ASC, identity, role ID, life state, and population member ID when present.
- Instigator actor, controller, and PlayerState.
- Source avatar, source role, and source weapon/effect causer.
- Ability tag and damage type.
- Death impulse and hit location when available.
- Server world time.
- Optional `BattleEventId` and `BattleZoneId` copied from the Day 04 effect-context fields, which default to `NAME_None` until Day 12 begins authority-only stamping.

The death event contains the same immutable attribution plus victim `DeathPolicyTag` and `DeathSequence`. Do not publish raw client-authored pointers, prices, role paths, or zone IDs.

Day 04 already declares and serializes `BattleEventId`/`BattleZoneId` in `FAuraGameplayEffectContext`. Day 11 does not add duplicate fields or replication bits; it verifies the existing accessors/`Duplicate`/`NetSerialize` path and copies the values into fatal/death events.

## Exactly-once policy dispatcher

`UAuraDeathPolicyDispatcher` is a server-only service owned by GameMode. It registers one handler for each supported native death-policy tag:

- `Death.PlayerRespawn`
- `Death.EnemyLoot`
- `Death.PopulationRespawn`

The dispatcher owns the single server-wide `FOnAuraAuthoritativeDeath OnAuthoritativeDeath` multicast delegate. GameMode creates the dispatcher before Actor BeginPlay. Systems bind once to this service and unbind during shutdown/EndPlay; they never enumerate and bind every actor's combat-state component.

The dispatcher accepts an event only after `TryEnterDying` returned `true`. It rejects an unsupported/mismatched policy before any consequence and logs actor, policy, and death sequence.

### Shared path

For every accepted event:

1. Cancel active attacks/abilities and reject further incoming damage through life state.
2. Stop movement and AI on authority.
3. Apply shared collision, ragdoll/dissolve/audio, debuff shutdown, and targetability presentation.
4. Run exactly one policy handler.
5. Broadcast the dispatcher's neutral `OnAuthoritativeDeath` event exactly once.
6. Transition Dying→Dead at the configured presentation point.

### PlayerRespawn policy

- Calls the existing `AAuraGameModeBase::PlayerDied` flow once.
- Preserves PlayerState/ASC progression and reapplies the validated role on the new pawn.
- Does not enter population management or enemy loot.

### EnemyLoot policy

- Moves `SpawnDataDrivenLoot`, XP reward emission, lifespan, and existing spawn-table destruction behavior behind this handler.
- Grants configured reward exactly once to the authoritative attributed source.
- Does not use `AAuraEnemy::Die` as a second reward boundary.

### PopulationRespawn policy

- Stops Civilian AI and releases Day 10 marker reservations.
- Produces no standard enemy XP or loot.
- Relies on the dispatcher's one shared neutral death-event broadcast; the existing Day 09 population manager binds once and calls `RecordAuthoritativeDeath` once for `Death.PopulationRespawn` events.
- That event handler marks the deterministic member slot dead but does not schedule refill until Day 13.
- Has no compile-time reference to `AAuraBattleDirector`. Day 12 subscribes to the neutral event after its actor exists.

Concrete actor classes may provide presentation assets/hooks, but they may not choose or execute a policy based on inheritance.

## Compatibility and caller migration

- `UAuraAttributeSet::HandleIncomingDamage` builds `FAuraFatalDamageContext` and calls `TryEnterDying`.
- Only when it returns `true` may the code call the dispatcher or emit XP/loot/death events.
- Remove the current unconditional “call `Die`, then `SendXPEvent`” sequence.
- Convert `ICombatInterface::Die` and actor `Die` overrides into authority-only compatibility wrappers that enter the same transition with a synthetic diagnostic context. They cannot bypass the winner check.
- Move `AAuraEnemy::SpawnDataDrivenLoot` and `SetLifeSpan` after the successful policy dispatch; repeated `Die` calls become no-ops.
- Update `AuraCombatRules` to reject Dying, Dead, and Respawning from both targeting and damage using the component, not the old `bDead` Boolean.
- Keep `bDead` only as a temporary presentation compatibility mirror if Blueprint assets still require it; it is not authoritative.

## Replication and late-join presentation

Replicated life state and death sequence are the durable truth. Reliable multicast may be retained only as a low-latency cosmetic hint.

`OnRep_LifeState` must idempotently reconcile:

- CharacterMovement and AI-visible movement state.
- Capsule/mesh/weapon collision and physics.
- Dissolve/ragdoll/death audio state without replaying one-shot rewards.
- Debug health bar and selection visibility.
- Debuff and passive Niagara shutdown.

A client that becomes relevant or joins after Dead must reconstruct the same corpse presentation from replicated state without relying on the original multicast. Repeated OnRep/multicast orderings must not double-play destructive presentation or re-enable collision.

## Implementation steps

1. Extend the Day 03 component and types; do not create a second state component.
2. Add the immutable fatal/death attribution structures.
3. Add the GameMode-owned policy dispatcher keyed strictly by replicated `DeathPolicyTag`, including its one global neutral server delegate and lifecycle-safe binding API.
4. Make the Alive→Dying winner the sole gate for all consequences.
5. Migrate Player respawn, Enemy loot/XP/lifespan, and Civilian population notification behind their handlers.
6. Update AttributeSet, compatibility `Die` callers, combat rules, AI shutdown, and population death recording.
7. Add idempotent OnRep/late-join presentation.
8. Add attribution and transition logs with death sequence.
9. Add the native and network tests below.

## Native automation

Add:

- `Aura.RoleBattle.Day11.TransitionWinner`
- `Aura.RoleBattle.Day11.RepeatedFatalDamage`
- `Aura.RoleBattle.Day11.DeathPolicyDispatch`
- `Aura.RoleBattle.Day11.EnemyRewardExactlyOnce`
- `Aura.RoleBattle.Day11.CivilianNoReward`
- `Aura.RoleBattle.Day11.NeutralDeathEventAttribution`
- `Aura.RoleBattle.Day11.CombatRulesRejectNonAlive`
- `Aura.RoleBattle.Day11.OnRepPresentationIdempotence`

The repeated-fatal test must submit simultaneous/sequential fatal effects and direct compatibility `Die` calls, then assert one transition, one sequence increment, one policy dispatch, one reward decision, and one neutral event.

## Current executable baseline smoke

`RunRoleBattleDay11NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`. The 2026-08-24 baseline passes both modes and records eight assertions: static contracts, coordinated world readiness, a real Civilian Alive→Dying winner, repeated-transition rejection, first dispatch plus duplicate rejection, population death accounting and AI stop, director replication on both clients, all three population states on both clients including late join, and no crash signature.

This baseline is executable lifecycle evidence, not the full Day 11 completion gate.

## Remaining completion acceptance matrix

Before Day 11 is marked complete, extend the runner or a successor to:

1. Kills one Player and Enemy from normal authoritative attributed damage; for the pre-Day-12 Civilian case, installs Day 03's `WITH_DEV_AUTOMATION_TESTS` authority-only trusted policy and proves a remote client cannot install or forge it.
2. Sends repeated fatal hits in the same and following frames.
3. Verifies Player respawn once, Enemy loot/XP/lifespan once, and Civilian population death record once with no standard reward.
4. Compares life state/death sequence on two clients.
5. Confirms Civilian BrainComponent/movement and all non-Alive targeting/damage stop.
6. Connects a late client after all three deaths and verifies durable state/presentation.
7. Verifies every neutral event carries source, ability, damage type, policy, victim, and sequence; battle fields remain `NAME_None` until Day 12.

The current baseline runner enforces bounded startup, lifecycle assertion, late-join, and teardown timeouts; returns nonzero on any implemented assertion, child-process, crash, timeout, duplicate event, or missing artifact; and stops only processes it created. The completion extension must retain those guarantees while adding Player/Enemy reward and attributed-damage coverage. Reports are written to `Saved/Reports/Day11-{Listen|Dedicated}.json` beside isolated process logs.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day11; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day11Automation.log'

& '.\RunRoleBattleDay11NetworkSmoke.ps1' -Mode Listen

& '.\RunRoleBattleDay11NetworkSmoke.ps1' -Mode Dedicated
```

## Completion gate

All fatal paths converge on one authority-only Alive→Dying winner. Only that winner executes the handler selected by `DeathPolicyTag`, grants any reward, publishes one attributed event through the single GameMode-owned dispatcher delegate, or schedules lifecycle work. Population/director subscribers bind once without missing later-spawned actors. Player, Enemy, and Civilian policies remain distinct without inheritance choosing policy, and existing plus late clients reconstruct identical non-Alive state. All build, native, listen, and dedicated gates pass.
