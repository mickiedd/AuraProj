# Day 02 - Add Combat Identity

## Execution status - 2026-08-08

Implemented and verified. The build, three focused Day 2 tests, all 17 native Aura tests, the Day 1 runtime regression, the original two-process replication smoke, and all 22 AuraAbilityGraph smoke checks pass. Full evidence is recorded in [Role-Battle-Day-02-Combat-Identity-2026-08-08.md](../../Reports/Role-Battle-Day-02-Combat-Identity-2026-08-08.md). The runner now has the stricter listen/dedicated two-client contract below; that expanded contract must be rerun before it is treated as verified evidence.

## Goal

Give every source or target combat avatar an explicit, replicated identity instead of relying on raw Player and Enemy actor tags. Transient damage carriers such as projectiles and effect actors do not own an identity; damage continues to use the source avatar's identity and attribution.

## New files

- Source/Aura/Public/Combat/AuraCombatIdentityComponent.h
- Source/Aura/Private/Combat/AuraCombatIdentityComponent.cpp
- Source/Aura/Public/Combat/AuraCombatTypes.h
- RunRoleBattleDay2NetworkSmoke.ps1

## Files to modify

- Source/Aura/Aura.Build.cs
- Source/Aura/Public/AuraGameplayTags.h
- Source/Aura/Private/AuraGameplayTags.cpp
- Source/Aura/Public/Character/AuraCharacterBase.h
- Source/Aura/Private/Character/AuraCharacterBase.cpp
- Source/Aura/Public/Character/AuraCharacter.h
- Source/Aura/Private/Character/AuraCharacter.cpp
- Source/Aura/Public/Character/AuraEnemy.h
- Source/Aura/Private/Character/AuraEnemy.cpp
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Private/AI/BTService_FindNearestPlayer.cpp
- Source/Aura/Private/Tests/AuraRoleBattleTests.cpp

## Concrete data contract

Define `FAuraCombatIdentity` as a `BlueprintType` `USTRUCT` in `AuraCombatTypes.h`. It contains:

- `FGameplayTag FactionTag`
- `FGameplayTag ControlTypeTag`
- `FGameplayTag CombatProfileTag`
- `FGameplayTag DeathPolicyTag`
- `bool bTargetable`
- `bool bCanAttack`
- `bool bCanBeDamaged`
- `bool bAllowFriendlyFire`

Every member is a reflected `UPROPERTY(EditAnywhere, BlueprintReadOnly)`. The four booleans are stored explicitly rather than inferred from faction or combat profile.

Register these native tags through the existing `FAuraGameplayTags` singleton:

- Faction.Player
- Faction.Enemy
- Faction.Civilian
- Control.Player
- Control.EnemyAI
- Control.CivilianAI
- Combat.Unassigned
- Combat.Magic
- Combat.Gun
- Combat.Civilian
- Death.PlayerRespawn
- Death.EnemyLoot
- Death.PopulationRespawn

`IsValid()` requires all four categorical tags to be valid. `Combat.Unassigned` is an intentional valid Day 2 profile for current Player and Enemy actors; Day 5 adds the explicit role-schema value and Day 6 applies it without hardcoded Aura/BungeeMan role-name inference. Move `GameplayTags` to `PublicDependencyModuleNames` because the new public combat type exposes `FGameplayTag`.

### Required downstream handoff

Day 2 is complete with `Combat.Unassigned`, but that value is transitional rather than a valid end state for Aura or BungeeMan. Day 5 must parse full role identity/profile tags, flags, and control compatibility. Day 6 must, on authority and before attributes or abilities are granted:

1. Validate the selected role against the actor shell/control type.
2. Build the final `FAuraCombatIdentity` from the validated role plus non-overridable actor-shell constraints.
3. Update the existing `UAuraCombatIdentityComponent` through its authority-only mutation path and force a replication update.
4. Verify clients receive `Combat.Magic` for Aura and `Combat.Gun` for BungeeMan along with the matching faction, control, death policy, and flags.

No client may derive authoritative combat identity from its local copy of RoleConfig. If the Day 6 handoff fails, role application and ability grants fail closed; they must not continue with `Combat.Unassigned`.

## Component and authority contract

- `UAuraCombatIdentityComponent` is created exactly once by `AAuraCharacterBase`; derived classes must not create another copy.
- The component calls `SetIsReplicatedByDefault(true)` and replicates one `FAuraCombatIdentity Identity` property with `ReplicatedUsing=OnRep_Identity`.
- `AAuraCharacterBase` owns an `EditDefaultsOnly` `FAuraCombatIdentity DefaultCombatIdentity`. Derived runtime default builders resolve native tags after tag initialization, and authority copies the result into the component during `BeginPlay`.
- Runtime mutation uses one authority-only C++ initializer/setter. Do not add a client setter or client-to-server identity RPC.
- The same authority-only setter is intentionally reused by the Day 6 spawn-time role handoff. It may update identity after `BeginPlay`, must call `ForceNetUpdate` (or the equivalent replication wake-up), and is not a live role-switch API.
- Expose const getters plus `HasValidIdentity()`. Add a static component lookup helper that accepts any `AActor` and uses `FindComponentByClass`; `IsNotFriend` and targeting code must not cast to a concrete character class.
- Default-constructed identity has invalid tags and all flags false. Missing/invalid identity warnings must be rate-limited to at most once per actor per runtime.

## Initial identities

| Actor | Faction | Control | Combat profile | Death policy | Targetable | Can attack | Can be damaged | Friendly fire |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| `AAuraCharacter` | Faction.Player | Control.Player | Combat.Unassigned | Death.PlayerRespawn | true | true | true | false |
| `AAuraEnemy` | Faction.Enemy | Control.EnemyAI | Combat.Unassigned | Death.EnemyLoot | true | true | true | false |
| Civilian test fixture | Faction.Civilian | Control.CivilianAI | Combat.Civilian | Death.PopulationRespawn | true | false | true | false |

## Compatibility and scope decisions

- Migrate `IsNotFriend` in `AuraAbilitySystemLibrary.cpp` but retain its signature. For Day 2 only, it is a narrow compatibility adapter: Player versus Enemy and Enemy versus Player return true; Player versus Player, Enemy versus Enemy, any pairing containing Civilian, self-targeting, null actors, and missing/invalid identities return false. Invalid identities produce the rate-limited diagnostic. Day 3 replaces this table with `AuraCombatRules`.
- Migrate `BTService_FindNearestPlayer.cpp` to find the nearest valid, targetable `Faction.Player` pawn through combat identity. Preserve the existing blackboard target/distance keys and the current null/max-distance output when no candidate exists. Do not call the candidate "hostile" or implement a relationship matrix in this service on Day 2; Day 3 replaces the temporary Player-faction filter with `AuraCombatRules::CanCombatTarget`.
- Keep existing Player and Enemy actor tags temporarily for unrelated compatibility code, but do not add any new tag-based combat or targeting checks.
- `AuraEffectActor.cpp` remains unchanged on Day 2 because it controls pickup eligibility rather than hostile targeting or damage attribution. Its `bApplyEffectsToEnemies` tag filter is explicitly deferred to the Day 20 compatibility cleanup.
- `AuraPlayerState` is not a Day 2 change. Retain and regression-test the Day 1 persistent-ASC attribute initialization guard, but do not add combat identity to PlayerState.

## Implementation steps

1. Add the native tags and the concrete `FAuraCombatIdentity` structure.
2. Add the replicated identity component, authority-only initialization, `OnRep_Identity`, validation, and generic actor lookup.
3. Create the component once in `AAuraCharacterBase` and initialize it from `DefaultCombatIdentity` on authority during `BeginPlay`.
4. Configure the exact Player and Enemy defaults from the table above.
5. Add const identity getters on `AAuraCharacterBase` for convenience while keeping generic consumers component-based.
6. Migrate `IsNotFriend` to the exact Day 2 compatibility truth table above.
7. Migrate `BTService_FindNearestPlayer` to the exact Player-identity filter above.
8. Add automation coverage for identity defaults/validation, generic lookup, the full compatibility truth table including null/self/Civilian/missing cases, and the Civilian fixture.
9. Add server/client identity logging through authority initialization and `OnRep_Identity`; include actor name and every replicated field.
10. Build and run all required smoke gates below.

## Required verification and smoke gates

Add these automation tests to `AuraRoleBattleTests.cpp`:

- `Aura.RoleBattle.Day2.IdentityDefaults`
- `Aura.RoleBattle.Day2.IsNotFriendCompatibility`
- `Aura.RoleBattle.Day2.CivilianIdentity`

Run, in order:

```powershell
& '.\build_test.bat'

& '.\BuildDedicatedServer.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day2; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day2Automation.log'

& '.\RunRoleBattleDay1Smoke.bat'

& '.\RunRoleBattleDay2NetworkSmoke.ps1' -Mode Both
```

The Day 2 network runner starts isolated hidden server/client processes on `StartupMap` for both listen and dedicated modes, with two clients per mode, and stops only the processes it created. It uses bounded startup/assertion/teardown timeouts, returns nonzero on any assertion, child-process, crash, timeout, or missing-artifact failure, and writes `Saved/Logs/Day02-{Listen|Dedicated}-{Server|Client1|Client2}.log` plus `Saved/Reports/Day02-{Listen|Dedicated}.json` with commands, exit codes, and assertion results. The runner directly asserts the first three checks below; the Day 1 smoke and focused automation own their corresponding non-network checks:

1. Confirm every spawned `AAuraCharacter` and `AAuraEnemy` reports a valid server identity.
2. Confirm each client receives the same four tags and four flags through `OnRep_Identity`.
3. Confirm Enemy AI still selects the nearest Player and writes `TargetToFollow` and `DistanceToTarget`.
4. Confirm Player-to-Enemy and Enemy-to-Player damage still work, existing `IsNotFriend`-guarded projectile/melee paths reject same-faction targets, and the Day 1 respawn vitals remain stable. The Day 1 smoke and focused automation own these checks; the indirect damage paths inventoried on Day 1 remain assigned to Day 4.
5. Confirm existing pickups still honor `bApplyEffectsToEnemies` through the dedicated pickup compatibility check; this is not inferred from the network runner's identity log assertions.

Record the build exit code, automation summary, Day 1 smoke result, network-smoke result/log paths, and any failure. Every command must pass before the completion gate is accepted.

## Completion gate

Every source or target combat avatar has a valid server-owned Day 2 identity, and clients receive the same identity. `IsNotFriend` and enemy Player acquisition no longer read Player/Enemy actor tags, all Day 2 automation tests pass, the Day 1 runtime smoke remains green, and the listen/dedicated multi-process Day 2 network smoke passes. Transient projectiles/effect actors continue using their source avatar for identity and attribution; pickup filtering remains explicitly deferred. `Combat.Unassigned` is accepted only for this completed transitional gate and must be eliminated for Aura/BungeeMan by the Day 6 gate.
