# Data-Driven Buff and Pickup System

**Status:** Implemented and validated on 2026-08-02.

## Overview

Buffs and pickups are configured in `Content/Config/GameplayEffects.json` and applied by `AAuraEffectActor` through native `UGameplayEffect` classes. Pickup Blueprint assets select effects by JSON name and no longer reference Gameplay Effect Blueprint UAssets.

The next migration—replacing the remaining Blueprint projectile and pickup actor behavior while retaining presentation assets—is specified in `Gameplay-Blueprint-Decoupling-Migration-Plan.md`.

The system supports:

- Instant, fixed-duration, and infinite effects.
- Periodic execution, including immediate or delayed first execution.
- Health, Mana, and every player-facing attribute in `UAuraAttributeSet`.
- Dynamic Gameplay Effect asset tags such as `Message.HealthPotion`.
- Infinite-effect removal when overlap ends.
- Runtime validation of malformed or unsupported configuration.

## Runtime Architecture

### Effect actor

`AAuraEffectActor` exposes three effect-name slots:

- `InstantEffectName`
- `DurationEffectName`
- `InfiniteEffectName`

`OnOverlap` and `OnEndOverlap` apply the configured name according to the corresponding `EEffectApplicationPolicy`. There is no legacy `TSubclassOf<UGameplayEffect>` fallback.

`ApplyDataDrivenEffect` performs the following sequence:

1. Rejects invalid targets, disallowed enemies, and empty effect names.
2. Loads `Content/Config/GameplayEffects.json`.
3. Finds the named entry under `pickupEffects`.
4. Validates its duration policy and required values.
5. Selects the appropriate native Gameplay Effect class before creating the spec.
6. Initializes every supported SetByCaller attribute to zero.
7. Overlays configured Health, Mana, and `attributes` values.
8. Applies period, duration, and dynamic asset tags to the spec.
9. Applies the spec to the target Ability System Component.
10. Tracks removable infinite handles or destroys the pickup for non-infinite effects.

Tracked infinite effects are removed on end-overlap and during `EndPlay`, preventing leaked effects when an actor or level is destroyed unexpectedly.

### Native Gameplay Effect classes

The native classes are declared in:

`Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/AuraAttributeGameplayEffect.h`

| Class | Duration policy | First periodic execution |
|---|---|---|
| `UAuraPickupGameplayEffect` | Instant | Not applicable |
| `UAuraPickupGameplayEffect_Duration` | HasDuration | Immediate |
| `UAuraPickupGameplayEffect_DurationDelayed` | HasDuration | After one period |
| `UAuraPickupGameplayEffect_Infinite` | Infinite | Immediate |
| `UAuraPickupGameplayEffect_InfiniteDelayed` | Infinite | After one period |

Each class contains SetByCaller modifiers for 20 supported attributes:

- Primary: Strength, Intelligence, Resilience, Vigor.
- Secondary: Armor, ArmorPenetration, BlockChance, CriticalHitChance, CriticalHitDamage, CriticalHitResistance, HealthRegeneration, ManaRegeneration, MaxHealth, MaxMana.
- Resistance: Fire, Lightning, Arcane, Physical.
- Vital: Health, Mana.

The class default objects are rebuilt by `UAuraAssetManager::StartInitialLoading` after native gameplay tags are initialized. This is required because the CDO constructors run before the project's native tags are populated.

Every modifier receives a SetByCaller magnitude when a spec is created. Unused modifiers are explicitly assigned zero; leaving them unset causes GAS error logs rather than silently behaving as zero.

## JSON Schema

Effects live under the `pickupEffects` object:

```json
{
  "pickupEffects": {
    "exampleBuff": {
      "duration": "duration",
      "durationValue": 5,
      "period": 1,
      "executeOnApplication": true,
      "health": 10,
      "mana": 0,
      "attributes": {
        "Attributes.Primary.Resilience": 15
      },
      "assetTags": [
        "Message.HealthPotion"
      ]
    }
  }
}
```

### Fields

| Field | Type | Required | Meaning |
|---|---|---|---|
| `duration` | string | Yes | `instant`, `duration`, or `infinite`. |
| `durationValue` | number | For `duration` | Lifetime in seconds; must be greater than zero. |
| `period` | number | No | Periodic execution interval in seconds; must be greater than zero and cannot be used with `instant`. |
| `executeOnApplication` | boolean | No | Whether a periodic effect executes immediately. Defaults to `true`. |
| `health` | number | No | Additive `Attributes.Vital.Health` magnitude. |
| `mana` | number | No | Additive `Attributes.Vital.Mana` magnitude. |
| `attributes` | object | No | Additional additive magnitudes keyed by a supported `Attributes.*` gameplay tag. |
| `assetTags` | string array | No | Registered gameplay tags appended to the effect spec. |

An `attributes` entry overrides `health` or `mana` if it uses the corresponding vital tag.

Unknown effect names, invalid policies, invalid duration/period values, nonnumeric attributes, unsupported attribute tags, and unknown asset tags are rejected or logged without crashing.

## Current Effects

| Name | Behavior | Source migrated |
|---|---|---|
| `healthPotion` | Instant `+50 Health`; `Message.HealthPotion` | `GE_PotionHeal` |
| `manaPotion` | Instant `+25 Mana`; `Message.ManaPotion` | `GE_PotionMana` |
| `healthCrystal` | `+1 Health` every `0.1s` for `4s`, including application; `Message.HealthCrystal` | `GE_CrystalHeal` |
| `manaCrystal` | `+1 Mana` every `0.1s` for `1s`, first tick delayed; `Message.ManaCrystal` | `GE_CrystalMana` |
| `fireArea` | Infinite `-5 Health` every `1s`, including application | `GE_FireArea` |
| `testAttribute` | Instant `+15 Resilience` | `GE_TestAttributeBased` |

The legacy values above were read from the actual Gameplay Effect CDOs in the Unreal Editor. `GE_FireArea` contained no burn tag, debuff, or Gameplay Cue requiring a separate implementation.

## Blueprint Migration

The following Blueprint class defaults were migrated and their overlap graphs were rewired to call `OnOverlap`:

- `BP_HealthPotion` -> `InstantEffectName = healthPotion`
- `BP_ManaPotion` -> `InstantEffectName = manaPotion`
- `BP_HealthCrystal` -> `DurationEffectName = healthCrystal`
- `BP_ManaCrystal` -> `DurationEffectName = manaCrystal`
- `BP_FireArea` -> `InfiniteEffectName = fireArea`
- `BP_TestActor` -> `InstantEffectName = testAttribute`

The legacy `ApplyEffectToTarget` function and `InstantGameplayEffectClass`, `DurationGameplayEffectClass`, and `InfiniteGameplayEffectClass` properties were removed from `AAuraEffectActor`.

The six legacy Gameplay Effect UAssets and their snapshots were deleted. The migrated Blueprint snapshots were also removed because they embedded recoverable copies of the pre-migration packages. Generate fresh snapshots from the Aura Editor snapshot tool when updated analysis snapshots are required.

## Web Configuration Editor

The Gameplay Effect configuration UI in `Plugins/AuraAbilityGraph/Editor/js/app.js` supports editing:

- Effect name.
- Health and Mana.
- Duration policy and duration seconds.
- Period and immediate/delayed execution.
- Comma-separated asset tags.
- An attribute-tag/value JSON object.

The saved output omits optional fields when they are unused.

## Testing and Validation

Focused automation tests are located at:

`Source/Aura/Private/Tests/AuraPickupGameplayEffectTests.cpp`

They validate:

- Native Instant, Duration, delayed-Duration, and Infinite policies.
- All 20 supported modifiers are present.
- Health SetByCaller application changes the target attribute.
- Duration and period overrides are retained on the spec.
- Infinite effects persist and can be removed by handle.
- `GameplayEffects.json` parses and contains every migrated entry.
- FireArea and arbitrary Resilience configuration values match the inspected assets.

### Build

```powershell
& 'C:\Git\UE_5.5\Engine\Build\BatchFiles\Build.bat' AuraEditor Win64 Development '-Project=C:\Git\AuraProj\Aura.uproject' -WaitMutex -NoHotReloadFromIDE
```

### Buff automation tests

```powershell
& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -unattended -nop4 -nullrhi -DisablePlugins=RiderLink '-ExecCmds=Automation RunTests Aura.Buffs' '-TestExit=Automation Test Queue Empty'
```

Expected result: three tests complete with `Result={Success}` (including cached pickup-definition cross-validation).

### Ability graph smoke suite

```powershell
& 'C:\Git\UE_5.5\Engine\Binaries\Win64\UnrealEditor-Cmd.exe' 'C:\Git\AuraProj\Aura.uproject' -AuraAbilityGraphSmokeTest -unattended -nop4 -nullrhi -DisablePlugins=RiderLink
```

Validated result on 2026-08-02: `18 passed, 0 failed`.

## Native pickup actor migration (2026-08-02)

`Content/Config/PickupDefinitions.json` now owns pickup collision, presentation, movement, effect mapping, and enemy-loot rates. `AAuraEffectActor` creates and binds its collision, mesh, and Niagara components natively and reads immutable cached definitions. `GameplayEffects.json` remains the source of effect behavior and magnitudes.

`StartupMap` was migrated and reloaded with 20 native configured actors: four each of the health/mana potions and crystals, three fire areas, and one test-attribute actor. Per-instance actor-level overrides were retained. Enemy loot now spawns the native class by definition name; `DA_LootTiers` no longer contains Blueprint class rows.

The eight historical pickup Blueprint packages (including both duplicate Potion/Crystal paths) were deleted after Asset Registry showed no outside referencers and a Windows cook succeeded. Presentation meshes, materials, sounds, and Niagara systems remain allowed UAsset dependencies.

## Corrections to the Original Plan

Deep inspection and runtime testing identified four assumptions that required correction:

1. Calling `SetDuration` on a spec created from an Instant Gameplay Effect does not reliably convert the definition into a duration effect. Separate native duration policies are required before spec creation.
2. The crystal and fire-area assets used periodic behavior and differing first-tick policies. Migrating only duration and magnitude would have changed gameplay.
3. Missing SetByCaller magnitudes produce GAS error logs. A shared all-attribute effect must initialize every unused magnitude to zero.
4. Five pickup Blueprints directly called the legacy function in their graphs. Changing only class defaults would not permit removal of the legacy reflected API; those graph nodes had to be migrated and the assets resaved first.
