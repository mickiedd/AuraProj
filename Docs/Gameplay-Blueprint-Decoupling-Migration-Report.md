# AuraAbilityGraph and Buff Blueprint-Decoupling Implementation Report

Date: 2026-08-02  
Status: Phases 1 and 2 runtime migration implemented; Phase 3/4 cleanup remains gated.

## Implemented architecture

- `ProjectileDefinitions.json` is parsed once and caches validated native classes, collision/movement values, and soft presentation references.
- `SpawnProjectile` and `SpawnProjectiles` accept `ProjectileDefinition`; active FireBolt, FireBlast, and FireGun XML use named native definitions.
- `AAuraProjectile` configures native components and replicates its definition name. FireBall no longer exposes `StartOutgoingTimeline`; its outbound/return behavior is server-authoritative native code.
- `PickupDefinitions.json` controls interaction, collision, presentation, movement, effect mapping, and loot rates. Effect behavior remains in `GameplayEffects.json` and is cached rather than reparsed per overlap.
- `AAuraEffectActor` owns native collision/mesh/Niagara components and overlap delegates. Enemy loot spawns it by definition name.
- `StartupMap` contains 20 migrated native actors and no legacy pickup actors after save/reload validation.

## Retained and deleted assets

Deleted after zero-referencer and cook gates: `BP_HealthPotion`, `BP_ManaPotion`, both historical `BP_HealthCrystal` and `BP_ManaCrystal` package pairs, `BP_FireArea`, and `BP_TestActor`.

Retained presentation UAssets include the potion/crystal meshes and materials, `NS_Fire`, `NS_Fire_3`, `NS_Fireball`, fire explosion systems, and projectile audio.

| Asset | Remaining referencer | Decision |
|---|---|---|
| `BP_FireBolt` | `GA_FireBolt`, `GA_EnemyFireBolt` | Retain until player legacy metadata and enemy ability are migrated |
| `BP_FireBall` | `GA_FireBlast` | Retain until legacy player ability cleanup |
| player `GA_*`, cost/cooldown GEs | `DA_AbilityInfo`, plus `BP_AuraCharacter` for `GA_FireBolt` | Retain; legacy UI/save/status consumers still call `GetAbilityInfo()` |

`Saved/AuraMigration/LegacyAbilityManifest.json` contains the current legacy player referencer/dependency inventory.

## Validation results

| Check | Result |
|---|---|
| AuraEditor Win64 Development build | Success |
| `Aura.Projectiles` | 1/1 passed |
| `Aura.Buffs` | 3/3 passed |
| AuraAbilityGraph smoke | 18/18 passed |
| Reloaded `StartupMap` | 20 native, 0 legacy pickup actors |
| Post-migration pickup Asset Registry | No map/spawner referencers |
| Windows focused cook (`StartupMap`) | Success, 0 errors; 66 pre-existing asset-version warnings |

## Debug iterations

| Iteration | Failure/signature | Root cause | Fix | Rerun result |
|---|---|---|---|---|
| Baseline | `Aura.Projectiles` matched 0 tests | Suite did not exist | Added assertion-based definition/component suite | 1/1 passed |
| Build 1 | ambiguous conditional `TSubclassOf` and incomplete Niagara type | conditional combined `TSubclassOf`/`UClass*`; missing include | explicit assignment and `NiagaraSystem.h` | build passed |
| Map migration 1 | editor spawn divide-by-zero | headless editor placement API was unsafe for the new actor | exposed a runtime deferred-spawn helper and used it from the migration utility | 20 replacements saved |
| Map migration 2 | Python `save_map` missing world argument | UE 5.5 API signature | passed editor world and asset path | save/reload validation passed |

## Deferred gates

The ordered implementation handoff is maintained in `Gameplay-Blueprint-Decoupling-Next-Moves.md`.

- Migrate remaining `GetAbilityInfo()` UI/save/status consumers to `URuntimeAbilityInfo` and role/definition data.
- Clear the `GA_FireBolt` reference in `BP_AuraCharacter`, then review legacy player GA/GE deletion as a small gated batch.
- Port `GA_EnemyFireBolt` before deleting `BP_FireBolt`.
- Add dedicated listen-server/client gameplay scenarios and a packaged executable launch; current validation covers authority guards, replication configuration, automation, map reload, and cook but is not a substitute for that multiplayer gate.
