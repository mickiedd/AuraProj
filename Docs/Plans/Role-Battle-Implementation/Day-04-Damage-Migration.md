# Day 04 - Migrate Every Damage Boundary

## Goal

Route every production damage request through `AuraCombatRules`, enforce the rule again at the authoritative final application boundary, and preserve complete attribution through direct, projectile, beam, radial, hitscan, melee, and periodic/debuff damage.

Day 4 begins by completing and checking in the expanded Day 1 inventory before changing a damage path. “All producers” includes native Blueprint-callable helpers, smoke-only producers, and Blueprint assets; it does not mean only AuraAbilityGraph nodes.

## Files to modify

### Core damage contract and context

- Source/Aura/Public/AuraAbilityTypes.h
- Source/Aura/Private/AuraAbilityTypes.cpp
- Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h
- Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp
- Source/Aura/Public/AbilitySystem/Abilities/AuraDamageGameplayAbility.h
- Source/Aura/Private/AbilitySystem/Abilities/AuraDamageGameplayAbility.cpp
- Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp
- Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp

### Native projectile and ability producers

- Source/Aura/Public/Actor/AuraProjectile.h
- Source/Aura/Private/Actor/AuraProjectile.cpp
- Source/Aura/Public/Actor/AuraFireBall.h
- Source/Aura/Private/Actor/AuraFireBall.cpp
- Source/Aura/Private/AbilitySystem/Abilities/AuraProjectileSpell.cpp
- Source/Aura/Private/AbilitySystem/Abilities/AuraFireBolt.cpp
- Source/Aura/Private/AbilitySystem/Abilities/AuraFireBlast.cpp
- Source/Aura/Private/Player/AuraPlayerController.cpp

### AuraAbilityGraph parameter builders and damage actions

- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AbilityDefinition.h
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AbilityDefinition.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ApplyDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/CauseDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ElectrocuteBeamNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/HitscanTraceNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ModularBeamNodes.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectilesNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/Nodes/Actions/SpawnShardsNode.h
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AuraAbilityGraphModule.cpp

### Tests and runner

- Source/Aura/Private/Tests/AuraRoleBattleTests.cpp
- Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Tests/TestDataAbility.cpp
- Docs/Reports/Role-Battle-Damage-Producer-Inventory.md
- RunRoleBattleDay4DamageSmoke.ps1

## Content and config to inspect or adjust if serialized pins change

- Content/Config/ProjectileDefinitions.json
- Content/Blueprints/AbilitySystem/Enemy/Abilities/GA_MeleeAttack.uasset
- Content/Blueprints/AbilitySystem/Enemy/Abilities/GA_MeleeAttack.snapshot.json
- Content/Blueprints/AbilitySystem/Aura/Abilities/Lightning/GA_Electrocute.uasset
- Content/Blueprints/AbilitySystem/Aura/Abilities/Lightning/GA_Electrocute.snapshot.json
- Content/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBlast/BP_FireBall.uasset
- Content/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBlast/BP_FireBall.snapshot.json
- Content/Blueprints/AbilitySystem/Aura/Abilities/Arcane/ArcaneShards/GA_ArcaneShards.uasset
- Content/Blueprints/AbilitySystem/Aura/Abilities/Arcane/ArcaneShards/GA_ArcaneShards.snapshot.json

Do not mechanically resave these assets if the internal function implementation is sufficient. If a function signature or pin changes, compile/resave each affected Blueprint and refresh its checked-in snapshot in the same milestone.

## Damage-request and attribution contract

Extend `FDamageEffectParams` with the stable ability tag and the trusted `FAuraCombatRuleContext` inputs needed to construct an authoritative query. Source and target avatars are resolved from their ASCs on the server; a client-provided actor, identity, policy snapshot, role, damage magnitude, or ability tag is never trusted as authority.

Extend `FAuraGameplayEffectContext` and its accessors with the attribution not already represented safely by the base `FGameplayEffectContext`:

- Stable ability tag.
- Damage type.
- Source role ID at application time.
- Source controller and PlayerState when present.
- Battle zone ID and battle event ID, initially unset until Day 12.

Use the base context’s instigator, effect causer, source object, actors, and hit result rather than duplicating them. Update `Duplicate` and `NetSerialize` together. Allocate explicit replication bits for every optional custom field, serialize object references through `UPackageMap`, initialize absent fields deterministically, propagate `bOutSuccess`, and add a round-trip test. Do not copy whole combat identities into every effect context; final validation resolves current server identity/state from the source and target actors.

Periodic/debuff effects must copy the original attribution fields instead of creating an unattributed context. Each effective periodic tick must still pass current life-state and combat-policy validation, so a source or target that is `Dying`, `Dead`, `Respawning`, or protected cannot continue producing effective damage.

## Authoritative validation boundaries

1. `ApplyDamageEffect` validates non-null ASCs/avatars, server authority, source ability ownership/profile, and `AuraCombatRules::CanDamage` before creating a spec. Rejection returns an invalid/empty result and never dereferences a missing ASC or spec.
2. `UAuraDamageGameplayAbility::CauseDamage` must stop applying a spec directly. It builds `FDamageEffectParams`, supplies its ability tag, and calls the shared boundary.
3. Projectile overlap/hit may run cosmetics on clients, but only the authoritative projectile resolves the current source avatar, creates trusted rule context, and requests damage.
4. Every graph action either exits on non-authority before producing damage or calls the authoritative shared boundary; the shared boundary remains mandatory even when a node already filtered a candidate.
5. `ExecCalc_Damage` performs the second server-side rule/state validation before emitting `IncomingDamage`. `AuraAttributeSet` treats a rejected/zero output as non-damage and keeps the fatal transition idempotent.
6. Direct Blueprint call sites for `CauseDamage`/`ApplyDamageEffect` are covered by the same internal boundary and are included in the asset-reference verification.
7. A client-only or client-predicted action may produce cosmetic prediction but cannot modify authoritative target health.

## Implementation steps

1. Re-run the Day 1 inventory searches for `IsNotFriend`, `ActorHasTag`, `ApplyDamageEffect`, `FDamageEffectParams`, `CauseDamage`, `ApplyGameplayEffectSpecToTarget`, `ApplyGameplayEffectSpecToSelf`, `IncomingDamage`, and `BuildDamageEffectParams`. Save the before/after inventory in the Day 4 evidence log.
2. Extend and round-trip-test the damage params/custom effect context, including serializer and duplicate behavior.
3. Implement the two authoritative validation boundaries and stable rejection logging behind a non-shipping development flag.
4. Route the native `CauseDamage` helper and all native projectile/ability producers through the shared request.
5. Update every listed AuraAbilityGraph builder/action with authority, attribution, and rejection handling. A node may prefilter for efficiency but cannot be the only permission check.
6. Preserve blocked, critical, resistance, knockback, radial falloff, debuff, and hit-react behavior for accepted damage.
7. Make `ExecCalc_Damage` safe when source/target avatar, `CharacterClassInfo`, coefficient table, or an individual curve is absent. Use documented neutral coefficients for non-enemy profiles; do not add Civilian to `ECharacterClass`.
8. Verify delayed debuff kills retain original source/controller/PlayerState/role/ability/damage attribution and create one death transition.
9. Compile any affected Blueprint and refresh its snapshot only if the C++ signature changed.

## Required automation

Add these native tests:

- `Aura.RoleBattle.Day4.EffectContextNetSerialize`
- `Aura.RoleBattle.Day4.SharedDamageBoundary`
- `Aura.RoleBattle.Day4.DirectCauseDamageBoundary`
- `Aura.RoleBattle.Day4.AuthorityRejection`
- `Aura.RoleBattle.Day4.AllProducerAttribution`
- `Aura.RoleBattle.Day4.NonAliveSourceAndTargetRejection`
- `Aura.RoleBattle.Day4.PeriodicAttributionAndRevalidation`
- `Aura.RoleBattle.Day4.SafeDamageProfile`
- `Aura.RoleBattle.Day4.PreservedDamageSemantics`

Before any migration edit, create `Docs/Reports/Role-Battle-Damage-Producer-Inventory.md` and one compile-time table in `AuraRoleBattleTests.cpp` with producer ID, exact source/asset path, category, production-versus-test-only classification, expected shared boundary, and expected authority. The producer test enumerates that table and fails when the report/table differ, a discovered producer is absent, or a production application lacks the shared/final boundary. `AuraPlayerController` smoke-only damage remains explicitly test-only and cannot be mistaken for production coverage. The network smoke must attempt a client-originated invalid damage request, not merely observe normal server combat.

Run:

```powershell
& '.\build_test.bat'

& "$env:UE_ENGINE_ROOT\Engine\Binaries\Win64\UnrealEditor-Cmd.exe" '.\Aura.uproject' -unattended -nop4 -nullrhi '-ExecCmds=Automation RunTests Aura.RoleBattle.Day4; Quit' '-TestExit=Automation Test Queue Empty' '-abslog=Saved/Logs/Day4Automation.log'

& '.\RunSmokeTest.bat'
& '.\RunRoleBattleDay1Smoke.bat'
& '.\RunRoleBattleDay3NetworkSmoke.ps1'
& '.\RunRoleBattleDay4DamageSmoke.ps1'
```

`RunRoleBattleDay4DamageSmoke.ps1` enforces bounded startup/assertion/teardown timeouts, returns nonzero on any assertion, process, crash, timeout, or missing-artifact failure, stops only processes it created, and writes isolated `Saved/Logs/Day04-{Listen|Dedicated}-{Server|Client1|Client2}.log` plus `Saved/Reports/Day04-{Listen|Dedicated}.json`. It verifies FireBolt, FireGun, Enemy melee, beam, radial, hitscan test fixture, projectile, and periodic damage; same-faction/default-protected targets; server/client health; attribution; and one death transition.

## Completion gate

The after-inventory contains no production damage application that bypasses `AuraCombatRules`; native `CauseDamage` is migrated; all custom context fields round-trip; accepted direct and periodic damage retain correct attribution; invalid/client/friendly/protected or any non-Alive source/target request changes no authoritative health; and existing damage math/feedback remains intact. Every named test, AuraAbilityGraph smoke, prior-day regression, and the two-process Day 4 damage smoke must pass with recorded log paths and exit codes.
