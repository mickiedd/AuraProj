# GAS-Version Abilities — Implementation Documentation

> Generated: 2026-07-31. Audited against the runtime code and current configuration on 2026-08-02. Covers all data-driven abilities implemented via the AuraAbilityGraph GAS rewrite.

---

## Architecture Overview

The GAS-version abilities use a **data-driven architecture** with two parallel systems:

### 1. Data-Driven Path (AuraAbilityGraph)

```
UAuraDataAbility (instanced per actor)
  └─ resolves a preloaded UAuraAbilityDefinition from the ability spec
       └─ creates and executes UAuraAbilityActionTask instances
```

- **Ability definitions** are XML files in `Content/AbilityDefinitions/`
- **UAuraDataAbility** (`DataAbility.h/cpp`) is the runtime activation class. It is `InstancedPerActor`. At activation it resolves an already-parsed definition from the ability spec's `SourceObject` (or the replicated ability tag fallback), creates runtime tasks from the definition's node tree, and executes them.
- **UAuraAbilityDefinition** (`AbilityDefinition.h/cpp`) is a transient UObject populated by `LoadFromXML()` while role configuration is loaded or reloaded. It holds identity tags, cost, cooldown, damage parameters, and the root graph node. Montage data belongs to `PlayMontage` nodes rather than to the definition itself.
- **Graph execution** uses `UAuraAbilityActionTask` (base), with concrete tasks for each node type (`WaitForTargetData`, `PlayMontage`, `WaitForMontageEvent`, `SpawnProjectile`, `SpawnProjectiles`, `ApplyDamage`, `CauseDamage`, `MulticastGunFX`, `HitscanTrace`, `FaceTarget`, `SpawnShards`, `ElectrocuteBeam`, `Wait`, `Sequence`).
- **Cost/Cooldown** are pure C++ `UGameplayEffect` subclasses (`UAuraManaCostGameplayEffect`, `UAuraCooldownGameplayEffect`) — no Blueprint UAssets needed.
- **Damage** uses `UAuraDamageGameplayEffect` (C++ `UGameplayEffect` with `UExecCalc_Damage`) — no per-damage-type GE UAsset.

### 2. Traditional C++ Path (Ability Classes)

Each ability also has a C++ class in `Source/Aura/Public/AbilitySystem/Abilities/` that extends the GAS hierarchy:

```
UGameplayAbility (UE5 GAS)
  └─ UAuraGameplayAbility
       ├─ UAuraDamageGameplayAbility
       │    ├─ UAuraProjectileSpell → UAuraFireBolt / UAuraFireGun
       │    ├─ UAuraBeamSpell → UElectrocute
       │    └─ (direct) → UArcaneShards / UAuraFireBlast
       ├─ UAuraSummonAbility
       └─ UAuraPassiveAbility
```

The retained C++ classes provide `BlueprintCallable` helpers for their Blueprint ability graphs (for example, `SpawnProjectiles()`, `FireGun()`, `SpawnFireBalls()`, and `CauseDamage()`). AuraAbilityGraph nodes implement their own equivalent runtime logic; they do not call those legacy ability-class helpers. `UAuraFireBolt`, `UAuraFireGun`, `UArcaneShards`, `UAuraFireBlast`, and `UElectrocute` remain available for the traditional compatibility path.

### 3. Configuration

| Source | File | Role |
|---|---|---|
| Ability definitions | `Content/AbilityDefinitions/*.xml` | Graph, cost, cooldown, damage, montage-node and projectile-definition selections |
| Projectile definitions | `Content/Config/ProjectileDefinitions.json` | Native projectile class, collision, movement, lifetime, mesh scale, and FX assets |
| UI metadata | `Content/Config/AbilityInfo.json` | Icons, materials, level requirements |
| Role mapping | `Content/Config/RoleConfig.json` | Which abilities map to which role (Aura, BungeeMan) |
| Gameplay tags | `Config/DefaultGameplayTags.ini` | Native tag declarations |
| Attributes/effects | `Content/Config/GameplayEffects.json` | Attribute defaults and named pickup/buff effects |
| Level progression | `Content/Config/LevelConfig.json` | Level requirements and rewards |

---

> 2026-08-02 update: active projectile graph nodes use named entries in `Content/Config/ProjectileDefinitions.json`. FireBolt resolves to native `AAuraProjectile`; FireBlast resolves to native `AAuraFireBall`, whose outbound/return movement no longer requires a Blueprint timeline. Legacy projectile/GA descriptions below document the retained compatibility path only; see `Gameplay-Blueprint-Decoupling-Migration-Report.md` for deletion blockers.

## Buff and Pickup Effects

`AAuraEffectActor` applies named entries from `Content/Config/GameplayEffects.json` through native `UGameplayEffect` classes. Blueprint pickup instances select an entry with `InstantEffectName`, `DurationEffectName`, or `InfiniteEffectName`; effect magnitudes no longer require a Gameplay Effect Blueprint asset.

Each `pickupEffects` entry supports `duration` (`instant`, `duration`, or `infinite`), `durationValue` for duration effects, optional `period` and `executeOnApplication` settings, convenient `health` and `mana` magnitudes, arbitrary supported values in an `attributes` gameplay-tag map, and optional `assetTags` such as `Message.HealthPotion`.

The runtime selects a native Instant, Duration, or Infinite `UAuraPickupGameplayEffect` variant before it creates the spec. Changing only a spec's duration does not convert an Instant Gameplay Effect definition into a persistent effect. Infinite handles configured with `RemoveOnEndOverlap` are tracked per target and removed when overlap ends.

---

## Ability Class Hierarchy

### Base Classes

```
UGameplayAbility (UE5 GAS)
  └─ UAuraGameplayAbility
       ├─ CheckCost() — skips cost check on non-authoritative clients
       ├─ GetManaCost() / GetCooldown() — read from GEs
       ├─ GetDescription() / GetNextLevelDescription() / GetLockedDescription()
       └─ StartupInputTag (FGameplayTag)

  └─ UAuraDamageGameplayAbility : UAuraGameplayAbility
       ├─ CauseDamage(AActor*) — applies damage via GE spec
       ├─ MakeDamageEffectParamsFromClassDefaults() — builds FDamageEffectParams
       ├─ GetDamageAtLevel()
       ├─ GetRandomTaggedMontageFromArray()
       └─ Properties: DamageEffectClass, DamageType, Damage (FScalableFloat),
            DebuffChance/Damage/Duration/Frequency, DeathImpulseMagnitude,
            KnockbackForceMagnitude, KnockbackChance, bIsRadialDamage,
            RadialDamageInner/OuterRadius

  └─ UAuraProjectileSpell : UAuraDamageGameplayAbility
       ├─ ActivateAbility() — override (currently empty base impl)
       ├─ SpawnProjectile() — server-only, spawns AAuraProjectile
       ├─ ProjectileClass (TSubclassOf<AAuraProjectile>)
       └─ NumProjectiles (int32)

  └─ UAuraBeamSpell : UAuraDamageGameplayAbility
       ├─ StoreMouseDataInfo() / StoreOwnerVariables() / TraceFirstTarget()
       ├─ StoreAdditionalTargets()
       ├─ PrimaryTargetDied / AdditionalTargetDied (BPImplementableEvent)
       ├─ EndAbility() — override, unbinds death delegates
       └─ Properties: MouseHitLocation, MouseHitActor, OwnerPlayerController,
            OwnerCharacter, MaxNumShockTargets, BoundDeathTargets

  └─ UAuraSummonAbility : UAuraGameplayAbility
       ├─ GetSpawnLocations() / GetRandomMinionClass()
       └─ Properties: NumMinions, MinionClasses, Min/MaxSpawnDistance, SpawnSpread

  └─ UAuraPassiveAbility : UAuraGameplayAbility
       ├─ ActivateAbility() — override
       └─ ReceiveDeactivate()
```

---

## 5 Data-Driven Abilities

---

### 1. FireBolt

#### GAS-Version (Traditional — How to Implement from Scratch)

**Step 1: Create the C++ class**

Extend `UAuraProjectileSpell` (which extends `UAuraDamageGameplayAbility` → `UAuraGameplayAbility` → `UGameplayAbility`). Add the FireBolt-specific properties:

```cpp
// AuraFireBolt.h
UCLASS()
class UAuraFireBolt : public UAuraProjectileSpell
{
    GENERATED_BODY()
public:
    virtual FString GetDescription(int32 Level) override;
    virtual FString GetNextLevelDescription(int32 Level) override;

    UFUNCTION(BlueprintCallable)
    void SpawnProjectiles(const FVector& ProjectileTargetLocation,
                          const FGameplayTag& SocketTag,
                          bool bOverridePitch, float PitchOverride,
                          AActor* HomingTarget);

protected:
    UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
    float ProjectileSpread = 90.f;

    UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
    int32 MaxNumProjectiles = 5;

    UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
    float HomingAccelerationMin = 1600.f;

    UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
    float HomingAccelerationMax = 3200.f;

    UPROPERTY(EditDefaultsOnly, Category = "FireBolt")
    bool bLaunchHomingProjectiles = true;
};
```

**Step 2: Create the Legacy Blueprint GA**

- In UE Editor, create a Blueprint `GA_FireBolt` that extends `UAuraFireBolt`
- Set the Blueprint's default properties:
  - `ProjectileClass` = `BP_FireBolt` (the projectile Blueprint asset)
  - `NumProjectiles` = 5
  - `DamageEffectClass` = shared legacy `GE_Damage`
  - `DamageType` = `Damage.Fire`
  - `Damage` = 50
  - `KnockbackForceMagnitude` = 2000, `KnockbackChance` = 10 for a nominal 10% chance
  - `DeathImpulseMagnitude` = 5000

**Step 3: Create GE UAssets**

Configure the retained legacy GE assets:
- `GE_Cost_FireBolt` — Extends `UGameplayEffect`, `Instant`, adds a `SetByCaller` modifier for Mana cost (10)
- `GE_Cooldown_FireBolt` — Extends `UGameplayEffect`, `HasDuration`, duration = 5.0s, adds `Cooldown.Fire.FireBolt` tag
- `GE_Damage` — The repository's shared legacy instant damage GE using `ExecCalc_Damage`; the ability supplies the Fire damage tag and magnitude

**Step 4: Build the Blueprint Ability Graph**

Wire up the ability tasks in the Blueprint graph:

```
[Event Activate] → WaitTargetData (targeting) → FaceTarget → PlayMontage (AM_Cast_FireBolt)
  → WaitGameplayEvent (Event.Montage.FireBolt) → SpawnProjectiles (up to 5, level-scaled, spread=90°, homing)
```

Each task uses the standard GAS ability task nodes:
- **WaitTargetData**: Uses the cursor trace to get `UGameplayAbilityTargetData_Handle`
- **FaceTarget**: Rotates the avatar toward the target
- **PlayMontage**: Plays the cast animation montage
- **WaitGameplayEvent**: Waits for the `Event.Montage.FireBolt` event fired by the anim notify
- **SpawnProjectile**: Calls the `SpawnProjectile` function on the ability, passing the target location and socket tag

**Step 5: Register in RoleConfig**

In `Content/Config/RoleConfig.json`, use `startupAbilities` for traditional Blueprint/C++ ability classes and `lmbAbility` for the traditional LMB class. The `startupAbilityDefinitions` and `lmbAbilityDefinition` fields are reserved for data-driven definitions:
```json
{
  "role": "Aura",
  "startupAbilities": [],
  "lmbAbility": "/Game/Blueprints/.../GA_FireBolt.GA_FireBolt_C"
}
```

**Step 6: Create the Projectile Blueprint**

Create/configure `BP_FireBolt` extending `AAuraProjectile`. The legacy ability writes `DamageEffectParams` onto the deferred projectile before `FinishSpawning()`; native `AAuraProjectile` resolves the impacted target ASC and applies the effect on overlap.

---

#### AuraAbilityGraph (Data-Driven) Flow

The XML definition drives everything instead of Blueprints:

**Step 1: Write the XML file**

`Content/AbilityDefinitions/FireBolt.xml` defines all parameters and the graph:
- Identity tags (abilityTag, inputTag, type)
- Cost (mana=10), cooldown (tag + duration=5)
- Damage (type, base, debuff, knockback, death impulse)
- Montage path and event tag
- Graph node tree (5 nodes in Sequence)

**Step 2: No GE UAssets needed**

Cost uses `UAuraManaCostGameplayEffect` (C++), cooldown uses `UAuraCooldownGameplayEffect` (C++), damage uses `UAuraDamageGameplayEffect` (C++). No Blueprint GEs to create or maintain.

**Step 3: No Blueprint ability needed**

The graph is defined in XML and executed by `UAuraDataAbility`, not by a Blueprint ability graph.

**Step 4: Select a named projectile definition**

The active graph uses `<property name="ProjectileDefinition" value="fireBolt"/>`. The name resolves through `Content/Config/ProjectileDefinitions.json`, where `fireBolt` selects native `AAuraProjectile` and its movement and FX configuration. `ProjectileClass` remains only as a compatibility fallback for graphs that do not use a named definition.

**Level-scaling note:** the traditional `UAuraFireBolt::SpawnProjectiles()` uses `Min(AbilityLevel, NumProjectiles)`. The current data-driven node uses the XML `Count` directly, so FireBolt currently spawns five projectiles at every ability level.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint extending `UAuraFireBolt` | No Blueprint; `UAuraDataAbility` generic class |
| **Graph wiring** | Blueprint task nodes visually wired | XML `<graph>` section defines node tree |
| **Cost GE** | Separate `GE_Cost_FireBolt` Blueprint UAsset | C++ `UAuraManaCostGameplayEffect` — no UAsset |
| **Cooldown GE** | Separate `GE_Cooldown_FireBolt` Blueprint UAsset | C++ `UAuraCooldownGameplayEffect` — no UAsset |
| **Damage GE** | Separate `GE_Damage` Blueprint UAsset (or per-type) | C++ `UAuraDamageGameplayEffect` with exec calc — no UAsset |
| **Parameter changes** | Edit in Blueprint editor | Edit XML file |
| **Reload behavior** | Recompile/reload BP | Re-parsed when role configuration is loaded or explicitly reloaded; existing granted specs can retain their previous definition object |
| **Projectile reference** | Class pointer in BP defaults | Named entry in `ProjectileDefinitions.json` |
| **Montage reference** | Object pointer in BP defaults | String path in XML (loaded via `LoadObject`) |
| **Targeting** | Blueprint task nodes | `<graph>` XML nodes |
| **Ability-specific definition** | Blueprint GA plus supporting assets | One XML definition, plus shared native nodes/GEs and shared projectile/VFX configuration |
| **Designer-friendly** | Yes (visual BP graph) | Yes (XML, editor tooling) |
| **New node types needed** | N/A | `SpawnProjectiles`, `SpawnShards`, `ElectrocuteBeam` |

---

### 2. FireGun

#### GAS-Version (Traditional — How to Implement from Scratch)

**Step 1: Create the C++ class**

Extend `UAuraProjectileSpell`. Add `FireGun()` method and muzzle FX properties:

```cpp
// AuraGun.h
UCLASS()
class UAuraFireGun : public UAuraProjectileSpell
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category = "FireGun")
    void FireGun(const FHitResult& CursorHitResult);

protected:
    UPROPERTY(EditDefaultsOnly, Category = "Gun|FX")
    TObjectPtr<UParticleSystem> MuzzleEffect;

    UPROPERTY(EditDefaultsOnly, Category = "Gun|FX")
    TObjectPtr<USoundBase> FireSound;
};
```

**Step 2: Create the Legacy Blueprint GA**

- Blueprint `GA_FireGun` extending `UAuraFireGun`
- Default properties: `ProjectileClass = BP_AuraBullet`, single projectile
- Set `MuzzleEffect` and `FireSound` in the Blueprint defaults
- `Damage = 5`, `DamageType = Damage.Physical`, no debuff, no knockback

**Step 3: Create GE UAssets**

- No cost GE is required for the current zero-mana FireGun; the repository has no `GE_Cost_FireGun` asset
- `GE_Cooldown_FireGun` — HasDuration, duration = 0.2s, Cooldown.Gun.Fire tag
- `GE_Damage` — the repository's shared legacy damage GE

**Step 4: Build the Blueprint Ability Graph**

```
Event Activate → WaitTargetData → FaceTarget → PlayMontage (AM_FireGun)
  → WaitGameplayEvent (Event.Montage.FireGun) → FireGun(CursorHitResult)
```

The Blueprint calls the native `UAuraFireGun::FireGun()` helper. On authority that helper calls `SpawnProjectile()` and then `AAuraCharacterBase::MulticastPlayGunFireFX()`; there is no separate legacy `MulticastGunFX` Blueprint graph node.

**Step 5: Register in RoleConfig**

In `RoleConfig.json`, set BungeeMan's `lmbAbility` to the `GA_FireGun` Blueprint class path. Set `weaponTipSocket = "Muzzle"` so the gun spawns from the correct socket.

**Step 6: Configure muzzle FX**

Set `MuzzleEffect` and `FireSound` on the legacy GA Blueprint. `UAuraFireGun::FireGun()` invokes the character's native `MulticastPlayGunFireFX()` RPC. The data-driven version invokes the same character RPC through its `MulticastGunFX` graph node.

---

#### AuraAbilityGraph (Data-Driven) Flow

`Content/AbilityDefinitions/FireGun.xml` replaces the Blueprint ability graph and its cooldown/damage GE dependencies for this path. Because mana cost is zero, `UAuraDataAbility::ApplyCost()` returns without creating a cost spec:

```xml
<ability name="FireGun" abilityTag="Abilities.Gun.Fire" inputTag="InputTag.LMB" type="Abilities.Type.Offensive">
  <cost mana="0"/>
  <cooldown tag="Cooldown.Gun.Fire" duration="0.2"/>
  <damage effectClass="" type="Damage.Physical" base="5" deathImpulseMagnitude="1000" knockbackChance="0"/>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage">
        <property name="Montage" value="/Game/Assets/Characters/Aura/Animations/Abilities/AM_FireGun.AM_FireGun"/>
      </node>
      <node class="WaitForMontageEvent">
        <property name="EventTag" value="Event.Montage.FireGun"/>
      </node>
      <node class="SpawnProjectile">
        <property name="SocketTag" value="CombatSocket.Weapon"/>
        <property name="ProjectileDefinition" value="fireGunBullet"/>
      </node>
      <node class="MulticastGunFX">
        <property name="MuzzleSocketTag" value="CombatSocket.Weapon"/>
        <property name="MuzzleEffect" value="..."/>
        <property name="FireSound" value="..."/>
      </node>
    </node>
  </graph>
</ability>
```

No GE Blueprint UAssets or Blueprint ability are required by the active data-driven path. The XML also depends on the shared `fireGunBullet` entry in `ProjectileDefinitions.json` and the referenced montage/FX assets. The `MulticastGunFX` node calls `AAuraCharacterBase::MulticastPlayGunFireFX()`.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint `GA_FireGun` extending `UAuraFireGun` | No Blueprint; generic `UAuraDataAbility` |
| **Graph wiring** | BP targeting/montage/event flow ending in native `FireGun()` | 6 XML action nodes in `<graph>` |
| **Cost GE** | None required for the current zero cost | `ApplyCost()` skips zero mana; shared C++ cost GE is used only for positive costs |
| **Cooldown GE** | `GE_Cooldown_FireGun` Blueprint (0.2s) | C++ `UAuraCooldownGameplayEffect` + XML duration |
| **Damage GE** | Shared `GE_Damage` Blueprint UAsset | C++ `UAuraDamageGameplayEffect` (shared) |
| **Muzzle FX** | Native `FireGun()` helper calls the character multicast | Built-in `MulticastGunFX` XML graph node calls the same character multicast |
| **Projectile spawn** | `SpawnProjectile` BP function call | `SpawnProjectile` XML graph node |
| **Ability-specific definition** | GA Blueprint plus supporting assets | One XML definition plus shared projectile/FX configuration |
| **Muzzle socket** | Hardcoded per-role in BP or `RoleConfig` | `CombatSocket.Weapon` tag via XML property |
| **Adding new gun variant** | Duplicate GA and adjust supporting assets | Add XML and select or add a named projectile definition |

---

### 3. ArcaneShards

#### GAS-Version (Traditional — How to Implement from Scratch)

**Step 1: Create the C++ class**

Extend `UAuraDamageGameplayAbility` directly (not projectile/beam — this is a direct-damage ability):

```cpp
// ArcaneShards.h
UCLASS()
class UArcaneShards : public UAuraDamageGameplayAbility
{
    GENERATED_BODY()
public:
    virtual FString GetDescription(int32 Level) override;
    virtual FString GetNextLevelDescription(int32 Level) override;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly)
    int32 MaxNumShards = 11;
};
```

**Step 2: Create the Legacy Blueprint GA**

- Blueprint `GA_ArcaneShards` extending `UArcaneShards`
- Default properties: `Damage = 30`, `DamageType = Damage.Arcane`; use `DebuffChance = 30` for a nominal 30% chance because the runtime uses percentage points, not a 0–1 fraction
- No `ProjectileClass` or `ProjectileSpell` inheritance — direct damage ability

**Step 3: Create GE UAssets**

- `GE_Cost_ArcaneShards` — Instant, mana cost = 20
- `GE_Cooldown_ArcaneShards` — HasDuration, 8.0s, tag `Cooldown.Arcane.ArcaneShards`
- `GE_Damage` — Shared legacy instant damage GE; Arcane damage/debuff values come from the ability defaults

**Step 4: Build the Blueprint Ability Graph (5 nodes)**

```
Event Activate → WaitTargetData → FaceTarget → PlayMontage (AM_Cast_ArcaneShards)
  → WaitGameplayEvent (Event.Montage.ArcaneShards) → SpawnActors (arcane shards)
```

The retained `GA_ArcaneShards` Blueprint contains the targeting, point sampling, actor spawning, cue, and damage wiring. At a high level it must:
1. Use a `SpawnActor` task or loop
2. Spawn up to `MaxNumShards` (11) actor instances, scaled by ability level
3. Each shard needs its own damage application (radial)
4. Each shard needs a gameplay cue for the visual effect (`GameplayCue.ArcaneShards`)
5. Sequence those effects for the selected ground points

**Step 5: Register in RoleConfig**

Add a traditional `GA_ArcaneShards` class path to Aura's `startupAbilities`. Use `startupAbilityDefinitions` only for the XML version.

---

#### AuraAbilityGraph (Data-Driven) Flow

`Content/AbilityDefinitions/ArcaneShards.xml`:

```xml
<ability name="ArcaneShards" abilityTag="Abilities.Arcane.ArcaneShards"
         inputTag="InputTag.2" type="Abilities.Type.Offensive">
  <cost mana="20"/>
  <cooldown tag="Cooldown.Arcane.ArcaneShards" duration="8"/>
  <damage ... type="Damage.Arcane" base="30" debuffChance="0.3" .../>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage">
        <property name="Montage" value="/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_ArcaneShards.AM_Cast_ArcaneShards"/>
      </node>
      <node class="WaitForMontageEvent">
        <property name="EventTag" value="Event.Montage.ArcaneShards"/>
      </node>
      <node class="SpawnShards">
        <property name="GameplayCueTag" value="GameplayCue.ArcaneShards"/>
      </node>
    </node>
  </graph>
</ability>
```

The `SpawnShards` node is implemented as `USpawnShardsNode` in C++. Its current behavior is:
- Spawn one configured `APointCollection` actor at the cursor location
- Sample `Min(MaxShards, Max(1, AbilityLevel))` ground-point locations
- Execute `GameplayCue.ArcaneShards` locally at each location
- Apply radial arcane damage at each location
- Destroy the temporary point collection when the task exits

`ShardClass` is parsed but is not currently loaded or spawned by `USpawnShardsTask`. Also, the cue uses `ExecuteGameplayCue_NonReplicated` after the server-authority check; a dedicated server therefore does not replicate that cue to clients. The XML node currently represents timed damage origins, not spawned shard actors with independent behavior.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint `GA_ArcaneShards` extending `UArcaneShards` | No Blueprint; `UAuraDataAbility` generic |
| **Shards spawn** | Blueprint spawns/configures shard visuals | Current C++ node samples locations but does not use `ShardClass` to spawn shard actors |
| **Radial damage per shard** | Manual in BP (spawn loop applies GE to each) | Handled by C++ `USpawnShardsNode` implementation |
| **Gameplay cue** | Blueprint-controlled cue/VFX | XML tag executed non-replicated on the authority path; dedicated-server clients need a replication fix |
| **Shards count scaling** | Manual Blueprint logic (`Min(Level, MaxNumShards)`) | Automatic in C++ `USpawnShardsNode` |
| **GE UAssets** | Legacy cost/cooldown plus shared damage GE | 0 for the data-driven path — uses shared C++ GEs |
| **Ability-specific definition** | GA Blueprint plus supporting assets | One XML definition plus shared native node and existing point/cue assets |
| **Adding new shard ability** | Duplicate + custom shard BP | Duplicate XML + adjust properties |

---

### 4. FireBlast

#### GAS-Version (Traditional — How to Implement from Scratch)

**Step 1: Create the C++ class**

Extend `UAuraDamageGameplayAbility` directly:

```cpp
// AuraFireBlast.h
UCLASS()
class UAuraFireBlast : public UAuraDamageGameplayAbility
{
    GENERATED_BODY()
public:
    virtual FString GetDescription(int32 Level) override;
    virtual FString GetNextLevelDescription(int32 Level) override;

    UFUNCTION(BlueprintCallable)
    TArray<AAuraFireBall*> SpawnFireBalls();

protected:
    UPROPERTY(EditDefaultsOnly, Category = "FireBlast")
    int32 NumFireBalls = 12;

private:
    UPROPERTY(EditDefaultsOnly)
    TSubclassOf<AAuraFireBall> FireBallClass;
};
```

**Step 2: Create the Legacy Blueprint GA**

- Blueprint `GA_FireBlast` extending `UAuraFireBlast`
- Default properties: `Damage = 60`, `DamageType = Damage.Fire`; use `DebuffChance = 50` for a nominal 50% chance because the runtime uses percentage points
- `NumFireBalls = 12`, `FireBallClass = BP_FireBall`

**Step 3: Create GE UAssets**

- `GE_Cost_FireBlast` — Instant, mana cost = 25
- `GE_Cooldown_FireBlast` — HasDuration, 10.0s, tag `Cooldown.Fire.FireBlast`
- `GE_Damage` — Shared legacy damage GE; Fire damage/debuff values come from the ability defaults

**Step 4: Build the Blueprint Ability Graph (1 root node)**

This is the simplest graph — just a single action node:

```
Event Activate → SpawnFireBalls()
```

But `SpawnFireBalls()` is complex internally (the C++ function does all the work):
- Calculate 12 evenly-spaced directions (360° spread)
- Spawn `AAuraFireBall` at the owner's location
- Set `ReturnToActor` and `Owner` for each fireball
- Each fireball gets `MakeDamageEffectParamsFromClassDefaults()`

In the traditional BP approach, the `SpawnFireBalls()` function had to be implemented as a C++ `UFUNCTION(BlueprintCallable)` or as a complex Blueprint macro — there's no visual node graph for it.

**Key difference from FireBolt:** FireBlast has **no montage, no WaitForTargetData, no FaceTarget, no WaitForMontageEvent**. It's a "fire and forget" ability with no targeting or cast time.

**Step 5: Register in RoleConfig**

Add a traditional `GA_FireBlast` class path to Aura's `startupAbilities` (not `lmbAbility`). Use `startupAbilityDefinitions` only for the XML version.

---

#### AuraAbilityGraph (Data-Driven) Flow

`Content/AbilityDefinitions/FireBlast.xml`:

```xml
<ability name="FireBlast" abilityTag="Abilities.Fire.FireBlast"
         inputTag="InputTag.1" type="Abilities.Type.Offensive">
  <cost mana="25"/>
  <cooldown tag="Cooldown.Fire.FireBlast" duration="10"/>
  <damage type="Damage.Fire" base="60" debuffChance="0.5" .../>
  <graph>
    <node class="Sequence" id="1">
      <node class="SpawnProjectiles" id="2">
        <property name="SocketTag" value="CombatSocket.Weapon"/>
        <property name="ProjectileDefinition" value="fireBall"/>
        <property name="Count" value="12"/>
        <property name="Spread" value="360"/>
        <property name="bHoming" value="false"/>
        <property name="bSetReturnToOwner" value="true"/>
      </node>
    </node>
  </graph>
</ability>
```

**Key Diff:** `ProjectileDefinition="fireBall"` resolves to native `AAuraFireBall` through `ProjectileDefinitions.json`. The `bSetReturnToOwner` property sets `ReturnToActor` and `Owner`; outbound distance/duration and return speed/distance come from the named projectile definition.

Also notable: **no `<montage>` element** — FireBlast has no cast animation. No targeting nodes either (no WaitForTargetData/FaceTarget). The simplest graph in the system.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint `GA_FireBlast` extending `UAuraFireBlast` | No Blueprint; `UAuraDataAbility` generic |
| **No montage/cast** | Must remember to omit PlayMontage/WaitForMontageEvent | XML naturally omits `<montage>` element |
| **No targeting** | Must remember to omit WaitForTargetData/FaceTarget | XML naturally omits these nodes |
| **360° spread** | Hardcoded in `UAuraAbilitySystemLibrary::EvenlySpacedRotators` call | XML property `Spread="360"` |
| **Return-to-owner** | C++ code: `FireBall->ReturnToActor = AvatarActor` | XML property `bSetReturnToOwner="true"` |
| **Fireball reference** | `TSubclassOf<AAuraFireBall>` in C++ defaults | Named `fireBall` definition selecting native `AAuraFireBall` |
| **GE UAssets** | Legacy cost/cooldown plus shared damage GE | 0 for the data-driven path — uses shared C++ GEs |
| **Ability-specific definition** | GA Blueprint plus supporting assets | One XML definition plus shared projectile/FX configuration |

---

### 5. Electrocute

#### GAS-Version (Traditional — How to Implement from Scratch)

**Step 1: Create the C++ class**

Extend `UAuraBeamSpell`:

```cpp
// Electrocute.h
UCLASS()
class UElectrocute : public UAuraBeamSpell
{
    GENERATED_BODY()
public:
    virtual FString GetDescription(int32 Level) override;
    virtual FString GetNextLevelDescription(int32 Level) override;
};
```

**Step 2: Create the Legacy Blueprint GA**

- Blueprint `GA_Electrocute` extending `UElectrocute`
- Default properties: `Damage = 10`, `DamageType = Damage.Lightning`
- `DebuffChance = 50`, `DebuffDamage = 2`, `DebuffDuration = 3`, `DebuffFrequency = 0.5` for a nominal 50% stun chance
- `MaxNumShockTargets = 5`

**Step 3: Create GE UAssets**

- `GE_Cost_Electrocute` — Instant, mana cost = 5
- `GE_Cooldown_Electrocute` — HasDuration, 3.0s, tag `Cooldown.Lightning.Electrocute`
- `GE_Damage` — Shared legacy damage GE; Lightning damage/debuff values come from the ability defaults

**Step 4: Build the Blueprint Ability Graph (5 nodes)**

```
Event Activate → WaitTargetData → FaceTarget → PlayMontage (AM_Cast_Electrocute)
  → WaitGameplayEvent (Event.Montage.Electrocute) → ElectrocuteBeam (channeled)
```

The `ElectrocuteBeam` step is the most complex — a **channeled ability** that:
1. Traces from caster to primary target
2. Spawns a Niagara beam effect along the trace
3. Repeatedly applies damage to the primary target (tick-based)
4. Chains to up to 5 additional nearby targets (`MaxNumShockTargets = 5`)
5. Each chain target gets a secondary beam segment
6. Continues channeling for `ChannelDuration` seconds (2.0s)
7. At `TickInterval` (0.2s), refreshes beam endpoints and damages the initially selected target set

In the traditional BP approach, this required:
- A custom Beam Actor or Component BP
- Timer-based tick logic in the ability
- Target chaining logic (find nearby enemies, trace to them)
- Niagara system spawning and parameter updates
- Damage application per tick to each target

**Step 5: Register in RoleConfig**

Add a traditional `GA_Electrocute` class path to Aura's `startupAbilities`. Use `startupAbilityDefinitions` only for the XML version.

**Step 6: Create beam VFX assets**

The beam Niagara system (`NS_ElectricBeam`), the ShockBurst GameplayCue, and the ShockLoop sound all had to be created as separate assets and wired up in the Blueprint.

---

#### AuraAbilityGraph (Data-Driven) Flow

`Content/AbilityDefinitions/Electrocute.xml`:

```xml
<ability name="Electrocute" abilityTag="Abilities.Lightning.Electrocute"
         inputTag="InputTag.3" type="Abilities.Type.Offensive">
  <cost mana="5"/>
  <cooldown tag="Cooldown.Lightning.Electrocute" duration="3"/>
  <damage type="Damage.Lightning" base="10" debuffChance="0.5" debuffDamage="2"
          debuffDuration="3" debuffFrequency="0.5" .../>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage">
        <property name="Montage" value="/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_Electrocute.AM_Cast_Electrocute"/>
      </node>
      <node class="WaitForMontageEvent">
        <property name="EventTag" value="Event.Montage.Electrocute"/>
      </node>
      <node class="ElectrocuteBeam"/>
    </node>
  </graph>
</ability>
```

The `ElectrocuteBeam` node channels a lightning beam with chain targets. `FindBeamTargets()` runs once when the node starts. It selects a primary target and up to `Min(MaxChainTargets, AbilityLevel - 1)` additional targets. Each server tick updates endpoints, removes invalid actor references, and applies damage to the remaining array; it does not search for replacement targets. A non-enemy cursor hit can receive a visual primary beam but has no ASC and therefore receives no damage or chain targets.

**Key Diff:** In the data-driven path, the orchestration is represented by a single `<node class="ElectrocuteBeam"/>`; the target selection, timers, damage, and Niagara logic live in the shared C++ task.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint `GA_Electrocute` extending `UElectrocute` | No Blueprint; `UAuraDataAbility` generic |
| **Channeled beam** | Blueprint graph logic for ticks, chains, and VFX | Single XML node backed by a shared C++ task |
| **Beam VFX** | Niagara system + custom BP actor | Handled by `ElectrocuteBeamTask` C++ class |
| **Target chaining** | Manual trace + target selection in BP | Initial selection in C++ (`MaxChainTargets=5`, `ChainRadius=850`); no per-tick reacquisition |
| **Tick-based damage** | Timer + loop in Blueprint | Handled by `UElectrocuteBeamTask::TickDamage()` |
| **Stun debuff** | Applied via GE spec in BP | Damage params come from XML, subject to the percentage-point issue documented below |
| **GE UAssets** | Legacy cost/cooldown plus shared damage GE | 0 for the data-driven path — uses shared C++ GEs |
| **Ability-specific definition** | GA Blueprint plus supporting assets | One XML definition plus the existing Niagara asset and shared C++ task |
| **Designer changes** | Editable in BP but complex to modify chain logic | Edit XML property values |

---

## Comparison Summary: Traditional GAS vs. AuraAbilityGraph

### Ability-Specific Definitions

The active data-driven setup replaces five GA Blueprint definitions with five XML files. This should not be interpreted as each ability depending on only one physical file: the runtime also depends on shared C++ graph nodes and GameplayEffects, `ProjectileDefinitions.json`, role/UI configuration, montages, Niagara/particle systems, sounds, and meshes. Exact percentage-based file savings are therefore not meaningful.

### Legacy GE UAssets Avoided by the Active Data Path

The current repository contains **10 relevant legacy GE Blueprint UAssets**: four `GE_Cost_*` assets, five `GE_Cooldown_*` assets, and one shared `GE_Damage`. There is no `GE_Cost_FireGun`, and damage is not represented by five separate per-ability assets. The active data-driven abilities do not require those legacy assets because they use the classes below. This does not prove every legacy asset is safe to delete while traditional and enemy abilities remain:
- `UAuraManaCostGameplayEffect` — shared by all mana-cost abilities
- `UAuraCooldownGameplayEffect` — shared by all cooldown abilities
- `UAuraDamageGameplayEffect` — shared by all damage abilities (with exec calc)

### Operational Differences

| Concern | Traditional GAS | AuraAbilityGraph |
|---|---|---|
| **Edit ability params** | Open Blueprint editor, find the right property | Edit XML text file |
| **Add new ability variant** | Create new GA BP + duplicate GEs + wire new graph | Copy XML, change values |
| **Balance changes** | Edit BP properties → recompile → test | Edit XML → explicitly reload role configuration or restart; verify newly granted specs use the new definition |
| **Designer access** | Needs UE Editor access + BP knowledge | Can edit XML in any text editor |
| **Runtime reload** | Requires recompile/reload | XML is parsed when role configuration loads/reloads, not on every activation |
| **Type safety** | Blueprint compile-time checking | Manual XML parsing with limited structural/property validation |
| **IDE support** | Full UE editor | Text editor; parsing is manual and no XML schema is currently enforced by the runtime |
| **Version control diffs** | Binary .uasset files (unreadable) | Plain text XML (human-readable) |
| **New node types** | C++ + Blueprint node registration | Add C++ node class + register in NodeRegistry |

---

## Passives (Legacy — Not Data-Driven)

These passives are **not yet ported** to the data-driven XML system. They remain as legacy Blueprint-based gameplay abilities:

| Passive | C++ Class | Tag |
|---|---|---|
| HaloOfProtection | UAuraPassiveAbility | `Abilities.Passive.HaloOfProtection` |
| LifeSiphon | UAuraPassiveAbility | `Abilities.Passive.LifeSiphon` |
| ManaSiphon | UAuraPassiveAbility | `Abilities.Passive.ManaSiphon` |

Reason: the current node registry has no nodes for the long-lived activation/deactivation and event-driven behavior used by these passive GameplayAbilities. They remain `UAuraPassiveAbility`-based Blueprint abilities; they are not themselves merely passive GameplayEffects.

---

## Enemy Abilities (Legacy — Not Data-Driven)

Still using legacy Blueprints. Not yet migrated:

| Ability | Blueprint |
|---|---|
| Enemy Fire Bolt | `GA_EnemyFireBolt` |
| Enemy Ranged Attack | `GA_RangedAttack` |
| Enemy Melee Attack | `GA_MeleeAttack` |
| Enemy Hit React | `GA_HitReact` |

---

## Known Issues (Relevant to Abilities)

Verified against the current implementation on 2026-08-02:

| Severity | Issue | Actual scope |
|---|---|---|
| High | XML uses fractional `debuffChance`/`knockbackChance`, while runtime rolls percentage points from 1 to 100 with a strict `<` comparison | Values below 1 currently never succeed. Debuffs are affected on ArcaneShards, FireBlast, and Electrocute; nonzero projectile knockback values below 1 are also ineffective. |
| High | `SpawnShards` parses `ShardClass` but never spawns it; its cue is non-replicated on an authority-only path | ArcaneShards visuals, particularly on a dedicated server |
| Medium | Knockback direction differs by node: some use target direction while projectile/hitscan/beam paths initially use `UpVector` | Behavior varies by the active damage path; `SpawnProjectiles` and `ElectrocuteBeam` must also be included in any standardization work |
| Medium | FireBolt projectile count does not scale with ability level in the data-driven path | FireBolt; XML `Count=5` is always used, unlike legacy `Min(Level, NumProjectiles)` |
| Medium | Electrocute selects chain targets once and removes only invalid actor references | Electrocute does not reacquire targets each tick and does not explicitly test the combat dead state during cleanup |
| Low | XML cooldown parsing supports only a constant, although runtime calls `GetValueAtLevel()` | All five definitions if level-scaled cooldowns are desired |
| Low | `PlayMontage` treats an empty montage as a successful no-op and a nonempty unloadable path as failure | Only graphs containing `PlayMontage`; FireBlast has no such node and is unaffected |
| Low | `ApplyDamage` and `CauseDamage` construct different damage contexts; `ApplyDamage` forces non-radial damage | Currently dormant for these five XML graphs because none uses either node |

`TargetAbilitySystemComponent = nullptr` at projectile spawn is intentional deferred targeting, not by itself a defect: `AAuraProjectile::OnSphereOverlap()` assigns the impacted target ASC before applying the effect.

---

## Phase 4 Remaining Work (Enemy Abilities + Cleanup)

- [ ] Port enemy abilities (`GA_EnemyFireBolt`, `GA_RangedAttack`, `GA_MeleeAttack`, `GA_HitReact`) to XML definitions
- [ ] Remove legacy `GE_Cost_*` / `GE_Cooldown_*` Blueprint assets (dead code for DataAbility path)
- [ ] Remove unused GE Blueprint assets after migration complete
- [ ] Depends on Phase 3 completing first (already done)
