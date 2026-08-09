# Day 03 - Add Combat Rules and the Minimal Combat-State Foundation

## Goal

Create one authoritative answer to “may this source combat-target or damage this target?” and introduce the minimum replicated life-state foundation required for that answer to reject actors as soon as death begins.

Combat targeting, AI threat selection, and interaction are separate contracts:

- `CanCombatTarget` answers combat permission only.
- AI threat eligibility evaluates `CanDamage(CandidateThreat, Observer, ServerResolvedContext)` in the candidate-to-observer direction. Later AI may rank permitted candidates, but distance or threat score must not redefine faction relationships.
- Interaction is not a combat-rule query. A later interaction interface may select a protected Civilian for conversation without making that Civilian a legal combat target.

Civilian damage is conservative/default-deny on Day 3. Day 12 supplies the authoritative battle-zone resolver that can opt a server query into a configured civilian-casualty rule.

## BungeeMan Gun Skill checkpoint

Apply the Day 3 combat-rule and life-state contract to FireGun's target and projectile impact path. A BungeeMan shot may damage only an authoritative permitted target that is still `Alive`; same-faction targets and targets already entering death remain rejected. Do not add a FireGun-specific bypass.

## Execution status — 2026-08-09

Implemented and verified in this checkout. The Day 3 native suite has eight passing tests; the combined `Aura.RoleBattle.Day` suite has all fourteen Day 1–3 tests passing. Day 1 smoke and both Listen/Dedicated Day 2 and Day 3 network smokes pass. Dedicated runners prefer the packaged `AuraServer.exe` when the cooked `StartupMap` exists and otherwise use the bounded UnrealEditor `-server` fallback; this checkout currently has no cooked server map.

## New files

- Source/Aura/Public/Combat/AuraCombatRuleContext.h
- Source/Aura/Public/Combat/AuraCombatRules.h
- Source/Aura/Private/Combat/AuraCombatRules.cpp
- Source/Aura/Public/Combat/AuraCombatStateComponent.h
- Source/Aura/Private/Combat/AuraCombatStateComponent.cpp
- RunRoleBattleDay3NetworkSmoke.ps1

## Files to modify

- Source/Aura/Public/Combat/AuraCombatTypes.h
- Source/Aura/Public/Combat/AuraCombatIdentityComponent.h
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Public/AuraGameplayTags.h
- Source/Aura/Private/AuraGameplayTags.cpp
- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Public/Game/AuraGameModeBase.h
- Source/Aura/Private/Game/AuraGameModeBase.cpp
- Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp
- Source/Aura/Private/Tests/AuraRoleBattleTests.cpp

## Combat-state contract

Define `EAuraCombatLifeState` with `Alive`, `Dying`, `Dead`, and `Respawning`. `UAuraCombatStateComponent` is created once by `AAuraCharacterBase`, replicates its state with `OnRep`, and exposes generic `FindForActor` and const query helpers.

Only authority may transition state. There is no client setter and no client-to-server state RPC. The allowed transitions are:

- `Alive -> Dying` through one idempotent `TryEnterDying` call.
- `Dying -> Dead` after the existing death presentation/cleanup starts.
- `Dead -> Respawning -> Alive` only when the same actor instance is deliberately reused.

Player replacement respawn is two actor lifecycles: the old pawn remains `Dead`; GameMode creates the replacement pawn initially as `Respawning`, initializes its role, ASC/avatar binding, attributes, and grants, then authority transitions that new pawn to `Alive`. It never changes the destroyed pawn back to Alive.

Repeated `TryEnterDying` calls return false and create no second death notification. Existing `ICombatInterface::IsDead` remains as a compatibility view that returns true for every non-Alive state (`Dying`, `Dead`, and `Respawning`); it must not be a second source of truth. Day 11 adds policy-specific rewards, corpse cleanup, and Civilian lifecycle behavior on top of this state machine rather than replacing it.

## Rule-context contract

Define an explicit `FAuraCombatRuleContext` containing:

- Query purpose: `CombatTargeting` or `Damage`.
- Trusted world context and relevant source/target or impact location.
- Optional server-resolved zone ID and battle-event ID.
- An optional immutable policy snapshot produced by an authoritative resolver.
- Explicit self-target intent for the small set of effects that legitimately target self.

Define a policy snapshot with validity, PvP permission, Player-to-Civilian permission, Enemy-to-Civilian permission, and target-protection state. Callers cannot set permissive booleans from client target data. On Day 3, an absent or invalid snapshot means:

- PvP denied.
- Player-to-Civilian damage and combat targeting denied.
- Enemy-to-Civilian damage and combat targeting denied.
- A Civilian source can never combat-target or damage another actor.

Day 12 implements the server-owned resolver and is the only milestone allowed to produce permissive zone snapshots. Rules must continue to default-deny if the resolver, zone, or snapshot is missing.

Provide one narrowly scoped `MakeTrustedTestPolicySnapshot` factory compiled only with `WITH_DEV_AUTOMATION_TESTS`. Native and multi-process test fixtures may install its result on authority to exercise pre-Day-12 Civilian damage, threat, and death integration. Production code, shipping builds, clients, command payloads, and ordinary `FDamageEffectParams` cannot call the factory or author the snapshot.

## Relationship and result contract

Return one structured result containing relationship (`Friendly`, `Hostile`, `Neutral`, or `Protected`), `bCanCombatTarget`, `bCanDamage`, and a stable rejection reason such as `None`, `InvalidSource`, `InvalidTarget`, `SelfDenied`, `SourceCannotAttack`, `SourceNotAlive`, `TargetNotTargetable`, `TargetNotDamageable`, `Friendly`, `Protected`, `PolicyDenied`, `Dying`, `Dead`, or `Respawning`.

Initial default matrix:

| Source | Player target | Enemy target | Civilian target |
| --- | --- | --- | --- |
| Player | Friendly; combat/damage denied | Hostile; allowed when flags/state permit | Protected; denied without a valid server policy snapshot |
| Enemy | Hostile; allowed when flags/state permit | Friendly; combat/damage denied | Protected; denied without a valid server policy snapshot |
| Civilian | Neutral; denied | Neutral; denied | Friendly; denied |

Friendly fire requires both a trusted policy that permits it and the applicable identity permission; it is disabled in the Day 3 default policy. Relationship type alone never implies that targeting or damage is permitted.

## Implementation steps

1. Add the life-state enum/component, authority-only transitions, replication, generic lookup, and table-driven transition tests.
2. Integrate the state component with the existing Player and Enemy death/respawn hooks while preserving current presentation and rewards. For replacement respawn, keep the old pawn Dead and hold the new pawn in Respawning until initialization completes. Do not perform the Day 11 policy refactor yet.
3. Add the rule-context and trusted-policy snapshot types with conservative defaults.
4. Implement `GetRelationship`, `CanCombatTarget`, `CanDamage`, and `CanReceiveDamage`. All actor overloads use generic identity/state component lookup; no concrete character cast or actor tag is allowed.
5. Reject null/missing/invalid identity, self-targeting without explicit intent, every non-Alive source or target including `Respawning`, a source that cannot attack, untargetable/undamageable targets, and friendly/protected/policy-denied targets with the exact reason.
6. Replace the Day 2 `IsNotFriend` truth table with a deprecated compatibility wrapper over `CanDamage` using the conservative Day 3 context. New code must call the explicit API.
7. Keep `BTService_FindNearestPlayer` scoped to Player candidates: require `Faction.Player`, then call `CanCombatTarget`, then choose the nearest permitted candidate. Do not broaden this legacy service to Civilians or call distance a relationship rule.
8. Add rate-limited diagnostics for invalid identities/states and rejected developer-mode queries; do not log every normal rejection in shipping builds.
9. Add the named automation and network coverage below.

## Required automation

Add to `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`:

- `Aura.RoleBattle.Day3.CombatStateTransitions`
- `Aura.RoleBattle.Day3.ReplacementPawnRespawnLifecycle`
- `Aura.RoleBattle.Day3.RelationshipMatrix`
- `Aura.RoleBattle.Day3.ConservativeCivilianPolicy`
- `Aura.RoleBattle.Day3.ThreatDirectionAndDefaultPolicy`
- `Aura.RoleBattle.Day3.IdentityFlagsAndLifeState`
- `Aura.RoleBattle.Day3.IsNotFriendCompatibility`
- `Aura.RoleBattle.Day3.EnemyPlayerTargeting`

The table tests must cover both directions of every matrix pair, null/missing/invalid identity, self, every identity flag, and `Alive`, `Dying`, `Dead`, and `Respawning` on both the source and target. They also cover an invalid/untrusted permissive snapshot. The test-only trusted factory must prove candidate-to-observer direction and must be inaccessible outside authority-side automation fixtures; production Day 3 runtime code must never produce a permissive Civilian snapshot.

`RunRoleBattleDay3NetworkSmoke.ps1` supports `-Mode Listen` and `-Mode Dedicated`; run both modes explicitly.

Run:

```powershell
& '.\build_test.bat'
& '.\BuildDedicatedServer.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds="Automation RunTests Aura.RoleBattle.Day; Quit"' '-abslog=Saved/Logs/Day3Automation.log'

& '.\RunRoleBattleDay1Smoke.bat'
& '.\RunRoleBattleDay2NetworkSmoke.ps1' -Mode Both
& '.\RunRoleBattleDay3NetworkSmoke.ps1' -Mode Listen
& '.\RunRoleBattleDay3NetworkSmoke.ps1' -Mode Dedicated
```

The Day 3 network runner must verify server-owned state transitions replicate to a client, a client cannot mutate state or policy, Enemy AI still writes `TargetToFollow`/`DistanceToTarget` for a Player, and all Civilian combat queries remain denied without a server policy resolver. It enforces bounded startup/assertion/teardown timeouts, returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure, stops only processes it starts, and writes `Saved/Logs/Day03-{Listen|Dedicated}-{Server|Client1|Client2}.log` plus `Saved/Reports/Day03-{Listen|Dedicated}.json`.

## Completion gate

One rules surface returns a stable reason for every combat-target and damage decision; interaction remains separate, and AI threat queries use the candidate-to-observer damage direction. The minimal replicated state machine rejects every non-Alive source and target, repeated transitions are idempotent, replacement pawns remain Respawning until initialized, and clients cannot author state or permissive rule context. Player/Enemy visible behavior remains unchanged, Civilian combat is default-deny outside the non-shipping authority test factory, all named tests pass, and the Day 1–2 regressions plus Day 3 network smoke are green.
