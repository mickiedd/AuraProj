# Role-Battle Damage-Producer Inventory

- Status: Day 4 step 1 — baseline inventory, captured before any migration edit.
- Date: 2026-08-09
- Scope: every production and test-only damage application in `Source/Aura` and `Plugins/AuraAbilityGraph`, plus the param builders that feed them.

## Methodology

Re-ran the Day 1 inventory searches across `Source/Aura` and `Plugins/AuraAbilityGraph`:

| Search term | Purpose |
|---|---|
| `ApplyDamageEffect` | shared damage entry point call sites |
| `CauseDamage` | direct ability damage helper |
| `BuildDamageEffectParams` | plugin shared param builder |
| `ApplyGameplayEffectSpecToTarget` / `ApplyGameplayEffectSpecToSelf` | direct spec application |
| `FDamageEffectParams` | param construction sites |
| `IsNotFriend` | legacy combat-rule pre-checks |
| `ActorHasTag` | tag-based filtering |
| `IncomingDamage` | final damage attribute producers/consumers |

## Producer table

This table is the canonical inventory. It is mirrored by the compile-time table in
`Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`; the `Aura.RoleBattle.Day4.ProducerInventory`
test fails when the two differ, when a discovered damage-application site is absent, or when a
production producer declares no shared boundary.

| Producer ID | Source / Asset Path | Category | Production / Test-only | Expected Shared Boundary | Expected Authority |
|---|---|---|---|---|---|
| Native.Projectile.AuraProjectile | Source/Aura/Private/Actor/AuraProjectile.cpp | Projectile | Production | ApplyDamageEffect | HasAuthority at impact |
| Native.Projectile.AuraFireBall | Source/Aura/Private/Actor/AuraFireBall.cpp | Projectile | Production | ApplyDamageEffect | HasAuthority at overlap |
| Native.Projectile.AuraProjectileSpell | Source/Aura/Private/AbilitySystem/Abilities/AuraProjectileSpell.cpp | Projectile (param builder) | Production | ApplyDamageEffect via projectile impact | HasAuthority at spawn |
| Native.Projectile.AuraFireBolt | Source/Aura/Private/AbilitySystem/Abilities/AuraFireBolt.cpp | Projectile (param builder) | Production | ApplyDamageEffect via projectile impact | HasAuthority at spawn |
| Native.Projectile.AuraFireBlast | Source/Aura/Private/AbilitySystem/Abilities/AuraFireBlast.cpp | Projectile (param builder) | Production | ApplyDamageEffect via projectile impact | HasAuthority at spawn |
| Native.Direct.CauseDamage | Source/Aura/Private/AbilitySystem/Abilities/AuraDamageGameplayAbility.cpp | Direct | Production | ApplyDamageEffect | Server ability activation |
| Native.Periodic.Debuff | Source/Aura/Private/AbilitySystem/AuraAttributeSet.cpp | Periodic | Production | Final AttributeSet revalidation | Server attribute execution |
| Smoke.Day1.AuraPlayerController | Source/Aura/Private/Player/AuraPlayerController.cpp | Smoke | Test-only | ApplyDamageEffect | HasAuthority (test-only) |
| Graph.ApplyDamage | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ApplyDamageNode.cpp | Direct | Production | ApplyDamageEffect | HasAuthority gate |
| Graph.CauseDamage | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/CauseDamageNode.cpp | Direct | Production | ApplyDamageEffect | HasAuthority gate |
| Graph.ElectrocuteBeam | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ElectrocuteBeamNode.cpp | Beam | Production | ApplyDamageEffect | HasAuthority gate |
| Graph.EnemyMeleeDamage | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/EnemyMeleeDamageNode.cpp | Melee | Production | ApplyDamageEffect | HasAuthority gate |
| Graph.HitscanTrace | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/HitscanTraceNode.cpp | Hitscan | Production | ApplyDamageEffect | HasAuthority gate |
| Graph.ApplyBeamDamage | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/ModularBeamNodes.cpp | Beam | Production | ApplyDamageEffect | bAuthorityOnly XML (must be enforced) |
| Graph.SpawnProjectile | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectileNode.cpp | Projectile | Production | ApplyDamageEffect via projectile impact | HasAuthority gate |
| Graph.SpawnProjectiles | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnProjectilesNode.cpp | Projectile | Production | ApplyDamageEffect via projectile impact | HasAuthority gate |
| Graph.SpawnShards | Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/Nodes/Actions/SpawnShardsNode.cpp | Radial | Production | ApplyDamageEffect | HasAuthority gate |

## Non-producer infrastructure

Files that legitimately contain a damage-application symbol but are not damage producers. Kept in
sync with the compile-time allowlist in `AuraRoleBattleTests.cpp`.

| File | Reason |
|---|---|
| Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp | defines ApplyDamageEffect + IsNotFriend |
| Source/Aura/Public/AbilitySystem/AuraAbilitySystemLibrary.h | declares the shared boundary |
| Source/Aura/Public/AuraAbilityTypes.h | defines FDamageEffectParams + FAuraGameplayEffectContext |
| Source/Aura/Private/AuraAbilityTypes.cpp | NetSerialize |
| Source/Aura/Public/AbilitySystem/AuraAttributeSet.h | declares IncomingDamage attribute |
| Source/Aura/Private/AbilitySystem/ExecCalc/ExecCalc_Damage.cpp | final boundary #2 (emits IncomingDamage) |
| Source/Aura/Public/AbilitySystem/Abilities/AuraDamageGameplayAbility.h | declares CauseDamage (cpp is the producer) |
| Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AbilityDefinition.h | declares BuildDamageEffectParams |
| Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AbilityDefinition.cpp | defines BuildDamageEffectParams |
| Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/AuraAbilityGraphModule.cpp | node registration + SmokeTest_DamageEffectParams |
| Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AuraDamageGameplayEffect.h | doc comment |
| Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/Nodes/Actions/SpawnShardsNode.h | doc comment |
| Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/Nodes/Actions/CauseDamageNode.h | class declaration |
| Source/Aura/Private/Character/AuraCharacterBase.cpp | non-damage attribute/effect application |
| Source/Aura/Private/Actor/AuraEffectActor.cpp | non-damage pickup/effect application |
| Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/DataAbility.cpp | non-damage cost/cooldown GEs |
| Source/Aura/Private/Tests/AuraRoleBattleTests.cpp | test fixture |
| Source/Aura/Private/Tests/AuraPickupGameplayEffectTests.cpp | test fixture |

## Before-migration baseline

All producers are **not migrated** at this baseline:

- `ApplyDamageEffect` (boundary #1) performs zero validation: it dereferences
  `SourceAbilitySystemComponent->GetAvatarActor()` and `TargetAbilitySystemComponent` without null
  checks, never checks authority or `AuraCombatRules::CanDamage`, and returns a context handle
  unconditionally.
- `UAuraDamageGameplayAbility::CauseDamage` bypasses the shared boundary entirely (direct
  `ApplyGameplayEffectSpecToTarget`).
- `AuraAttributeSet::Debuff` fabricates an unattributed context and applies periodic damage directly.
- `ExecCalc_Damage` (boundary #2) emits `IncomingDamage` with no rule/state re-validation and unsafe
  null derefs on missing avatars / `CharacterClassInfo` / coefficient curves.
- `FDamageEffectParams` and `FAuraGameplayEffectContext` lack the Day 4 ability-tag / attribution /
  rule-context fields.

## After-migration status

- `ApplyDamageEffect` now rejects invalid ASCs/avatars, non-authority requests, invalid specs, and
  requests denied by `FAuraCombatRules::CanDamage` before creating a spec.
- `UAuraDamageGameplayAbility::CauseDamage`, native projectile impacts, AuraAbilityGraph direct,
  beam, melee, hitscan, radial producers, and the BungeeMan gun projectile path now retain stable
  ability/context attribution and enter through the shared server boundary.
- `ExecCalc_Damage` and `UAuraAttributeSet` revalidate current source/target authority, identity,
  relationship, and life state; missing profiles/tables/curves use neutral coefficients.
- Periodic/debuff application duplicates the original custom effect context and performs current
  combat-rule validation before applying a tick.
- The compile-time producer table and this report are checked by
  `Aura.RoleBattle.Day4.ProducerInventory`.

## Evidence log

- Inventory searches re-run 2026-08-09 (see Methodology).
- Compile-time table + `Aura.RoleBattle.Day4.ProducerInventory` test added to
  `Source/Aura/Private/Tests/AuraRoleBattleTests.cpp`.
- `build_test.bat`: passed on 2026-08-10.
- `Aura.RoleBattle.Day4` automation: 10/10 tests passed; log at
  `Saved/Logs/Day4Automation.log`.
- `RunRoleBattleDay4DamageSmoke.ps1 -Mode Listen`: passed; artifacts at
  `Saved/Reports/Day04-Listen.json` and `Saved/Logs/Day04-Listen-{Server|Client1|Client2}.log`.
- `RunRoleBattleDay4DamageSmoke.ps1 -Mode Dedicated`: passed using the editor-server fallback
  because no cooked `StartupMap`/packaged server was available; artifacts at
  `Saved/Reports/Day04-Dedicated.json` and `Saved/Logs/Day04-Dedicated-{Server|Client1|Client2}.log`.
