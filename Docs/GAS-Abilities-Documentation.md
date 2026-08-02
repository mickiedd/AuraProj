# GAS-Version Abilities — Implementation Documentation

> Generated: 2026-07-31. Covers all data-driven abilities implemented via the AuraAbilityGraph GAS rewrite.

---

## Architecture Overview

The GAS-version abilities use a **data-driven architecture** with two parallel systems:

### 1. Data-Driven Path (AuraAbilityGraph)

```
UAuraDataAbility (instanced per actor)
  └─ reads UAuraAbilityDefinition (transient, parsed from XML)
       └─ executes UAuraAbilityActionTask graph (Sequence → children)
```

- **Ability definitions** are XML files in `Content/AbilityDefinitions/`
- **UAuraDataAbility** (`DataAbility.h/cpp`) is the runtime activation class. It is `InstancedPerActor`, reads the XML definition at activation, builds a task graph from the `<graph>` section, and executes it.
- **UAuraAbilityDefinition** (`AbilityDefinition.h/cpp`) is a transient UObject populated by `LoadFromXML()`. It holds all parsed XML data: tags, cost, cooldown, damage params, montage, and the root graph node.
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

The C++ classes provide the `BlueprintCallable` function-provider layer that graph nodes invoke (for example, `SpawnProjectiles()`, `FireGun()`, `SpawnFireBalls()`, and `CauseDamage()`). `UAuraFireBolt`, `UAuraFireGun`, `UArcaneShards`, `UAuraFireBlast`, and `UElectrocute` remain available for the traditional path while the ownership decision in `Docs/GAS-Migration-TODOs.md` remains open.

### 3. Configuration

| Source | File | Role |
|---|---|---|
| Ability definitions | `Content/AbilityDefinitions/*.xml` | Graph, cost, damage, montage, projectile params |
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
  - `DamageEffectClass` = `GE_Damage` (a separate GE UAsset for Fire damage)
  - `DamageType` = `Damage.Fire`
  - `Damage` = 50
  - `KnockbackForceMagnitude` = 2000, `KnockbackChance` = 0.1
  - `DeathImpulseMagnitude` = 5000

**Step 3: Create GE UAssets**

Create three separate GE Blueprint UAssets:
- `GE_Cost_FireBolt` — Extends `UGameplayEffect`, `Instant`, adds a `SetByCaller` modifier for Mana cost (10)
- `GE_Cooldown_FireBolt` — Extends `UGameplayEffect`, `HasDuration`, duration = 5.0s, adds `Cooldown.Fire.FireBolt` tag
- `GE_Damage_FireBolt` — Extends `UGameplayEffect`, `Instant`, uses `ExecCalc_Damage`, sets Fire damage type

**Step 4: Build the Blueprint Ability Graph**

Wire up the ability tasks in the Blueprint graph:

```
[Event Activate] → WaitTargetData (targeting) → FaceTarget → PlayMontage (AM_Cast_FireBolt)
  → WaitGameplayEvent (Event.Montage.FireBolt) → SpawnProjectile (x5, spread=90°, homing)
```

Each task uses the standard GAS ability task nodes:
- **WaitTargetData**: Uses the cursor trace to get `UGameplayAbilityTargetData_Handle`
- **FaceTarget**: Rotates the avatar toward the target
- **PlayMontage**: Plays the cast animation montage
- **WaitGameplayEvent**: Waits for the `Event.Montage.FireBolt` event fired by the anim notify
- **SpawnProjectile**: Calls the `SpawnProjectile` function on the ability, passing the target location and socket tag

**Step 5: Register in RoleConfig**

In `Content/Config/RoleConfig.json`, add the Blueprint class reference to the Aura role:
```json
{
  "role": "Aura",
  "startupAbilityDefinitions": ["/Game/Blueprints/.../GA_FireBolt.GA_FireBolt_C"],
  "lmbAbility": "/Game/Blueprints/.../GA_FireBolt.GA_FireBolt_C"
}
```

**Step 6: Create the Projectile Blueprint**

Create `BP_FireBolt` extending `AAuraProjectile`. Set its `DamageEffectParams` in the overlap handler using `MakeDamageEffectParamsFromClassDefaults()`.

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

**Step 4: No projectile Blueprint asset reference needed in the ability**

The `ProjectileClass` property in SpawnProjectiles references the projectile BP path as a string in XML, not as a UAsset reference.

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
| **Hot reload** | Recompile BP | Re-parse XML at runtime |
| **Projectile reference** | Class pointer in BP defaults | String path in XML |
| **Montage reference** | Object pointer in BP defaults | String path in XML (loaded via `LoadObject`) |
| **Targeting** | Blueprint task nodes | `<graph>` XML nodes |
| **File count per ability** | ~6 files (GA, 3 GEs, BP class, Projectile BP) | ~1 file (XML) + shared C++ GEs |
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

- `GE_Cost_FireGun` — Instant, mana cost = 0 (trivial/no-op)
- `GE_Cooldown_FireGun` — HasDuration, duration = 0.2s, Cooldown.Gun.Fire tag
- `GE_Damage_Physical` — Instant physical damage GE (shared with other physical damage abilities)

**Step 4: Build the Blueprint Ability Graph (6 nodes)**

```
Event Activate → WaitTargetData → FaceTarget → PlayMontage (AM_FireGun)
  → WaitGameplayEvent (Event.Montage.FireGun) → SpawnProjectile (single, toward cursor)
  → MulticastGunFX (play muzzle flash + fire sound)
```

The 6th node (`MulticastGunFX`) is a custom Blueprint node that fires the muzzle effect on all clients. In the legacy system, this had to be implemented as a custom GraphNode or a call to a static function library.

**Step 5: Register in RoleConfig**

In `RoleConfig.json`, set BungeeMan's `lmbAbility` to the `GA_FireGun` Blueprint class path. Set `weaponTipSocket = "Muzzle"` so the gun spawns from the correct socket.

**Step 6: Implement MulticastGunFX**

In the legacy system, muzzle FX multicast requires either:
- A custom Blueprint GraphNode
- Or C++ with `UFUNCTION(NetMulticast)` on the character class (`MulticastPlayGunFX`)
- The data-driven version handles this via the `MulticastGunFX` graph node built into the system

---

#### AuraAbilityGraph (Data-Driven) Flow

`Content/AbilityDefinitions/FireGun.xml` replaces the entire Blueprint ability + 3 GE assets:

```xml
<ability name="FireGun" abilityTag="Abilities.Gun.Fire" inputTag="InputTag.LMB" type="Abilities.Type.Offensive">
  <cost mana="0"/>
  <cooldown tag="Cooldown.Gun.Fire" duration="0.2"/>
  <damage effectClass="" type="Damage.Physical" base="5" deathImpulseMagnitude="1000" knockbackChance="0"/>
  <montage path="..." eventTag="Event.Montage.FireGun"/>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage"/>
      <node class="WaitForMontageEvent">
        <property name="EventTag" value="Event.Montage.FireGun"/>
      </node>
      <node class="SpawnProjectile">
        <property name="SocketTag" value="CombatSocket.Weapon"/>
        <property name="ProjectileClass" value="/Script/Aura.AuraProjectile"/>
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

No GE Blueprint UAssets, no Blueprint ability — just the XML file. The `MulticastGunFX` node is a built-in graph node in the AuraAbilityGraph plugin that handles the netmulticast call to `AAuraCharacterBase::MulticastPlayGunFX()` automatically.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint `GA_FireGun` extending `UAuraFireGun` | No Blueprint; generic `UAuraDataAbility` |
| **Graph wiring** | 6 BP task nodes wired visually | 6 XML graph nodes in `<graph>` |
| **Cost GE** | `GE_Cost_FireGun` Blueprint (mana=0, no-op) | C++ `UAuraManaCostGameplayEffect` |
| **Cooldown GE** | `GE_Cooldown_FireGun` Blueprint (0.2s) | C++ `UAuraCooldownGameplayEffect` + XML duration |
| **Damage GE** | `GE_Damage_Physical` Blueprint UAsset | C++ `UAuraDamageGameplayEffect` (shared) |
| **Muzzle FX** | Custom `MulticastGunFX` BP node or C++ net multicast | Built-in `MulticastGunFX` XML graph node |
| **Projectile spawn** | `SpawnProjectile` BP function call | `SpawnProjectile` XML graph node |
| **File count** | ~6 files (GA + 3 GEs + Projectile BP + character FX) | ~1 file (XML) |
| **Muzzle socket** | Hardcoded per-role in BP or `RoleConfig` | `CombatSocket.Weapon` tag via XML property |
| **Adding new gun variant** | Duplicate BA, duplicate GEs, modify BP | Add new XML file, reference new projectile class |

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
- Default properties: `Damage = 30`, `DamageType = Damage.Arcane`, `DebuffChance = 0.3`
- No `ProjectileClass` or `ProjectileSpell` inheritance — direct damage ability

**Step 3: Create GE UAssets**

- `GE_Cost_ArcaneShards` — Instant, mana cost = 20
- `GE_Cooldown_ArcaneShards` — HasDuration, 8.0s, tag `Cooldown.Arcane.ArcaneShards`
- `GE_Damage_Arcane` — Instant, Arcane damage type with debuff (stun)

**Step 4: Build the Blueprint Ability Graph (5 nodes)**

```
Event Activate → WaitTargetData → FaceTarget → PlayMontage (AM_Cast_ArcaneShards)
  → WaitGameplayEvent (Event.Montage.ArcaneShards) → SpawnActors (arcane shards)
```

The `SpawnActors` step in the Blueprint was the most complex part — you had to:
1. Use a `SpawnActor` task or loop
2. Spawn `MaxNumShards` (11) actor instances
3. Each shard needs its own damage application (radial)
4. Each shard needs a gameplay cue for the visual effect (`GameplayCue.ArcaneShards`)
5. Each shard needs a return-to-owner behavior (they orbit and explode)

In the legacy system, this required a custom Blueprint Library function or multiple Blueprint nodes to spawn, configure, and track each shard actor.

**Step 5: Register in RoleConfig**

Add to Aura's `startupAbilityDefinitions`.

---

#### AuraAbilityGraph (Data-Driven) Flow

`Content/AbilityDefinitions/ArcaneShards.xml`:

```xml
<ability name="ArcaneShards" abilityTag="Abilities.Arcane.ArcaneShards"
         inputTag="InputTag.2" type="Abilities.Type.Offensive">
  <cost mana="20"/>
  <cooldown tag="Cooldown.Arcane.ArcaneShards" duration="8"/>
  <damage ... type="Damage.Arcane" base="30" debuffChance="0.3" .../>
  <montage path="..." eventTag="Event.Montage.ArcaneShards"/>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage"/>
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

The `SpawnShards` node is a **new XML node type** added in Phase 3. It's implemented as `USpawnShardsNode` in C++ and handles:
- Spawning up to `MaxNumShards` (11) shard actors
- Applying radial arcane damage at each shard origin
- Firing the `GameplayCue.ArcaneShards` gameplay cue for VFX
- Automatically scales shard count with ability level

**Key Diff:** In the traditional GAS path, spawning 11 shards with damage + VFX required custom Blueprint logic (spawn loop + per-shard damage setup + gameplay cue). In the data-driven path, it's a single `<node class="SpawnShards"/>` in the XML.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint `GA_ArcaneShards` extending `UArcaneShards` | No Blueprint; `UAuraDataAbility` generic |
| **Shards spawn** | Custom BP logic: loop + spawn + per-shard setup | Single `SpawnShards` XML graph node |
| **Radial damage per shard** | Manual in BP (spawn loop applies GE to each) | Handled by C++ `USpawnShardsNode` implementation |
| **Gameplay cue** | Manual `K2_SpawnEmitterAtLocation` or BP task | `GameplayCueTag` XML property on SpawnShards node |
| **Shards count scaling** | Manual Blueprint logic (`Min(Level, MaxNumShards)`) | Automatic in C++ `USpawnShardsNode` |
| **GE UAssets** | 3 separate GE BPs needed | 0 — uses shared C++ GEs |
| **File count** | ~5 files (GA + 3 GEs + custom shard logic) | ~1 file (XML) |
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
- Default properties: `Damage = 60`, `DamageType = Damage.Fire`, `DebuffChance = 0.5`
- `NumFireBalls = 12`, `FireBallClass = BP_FireBall`

**Step 3: Create GE UAssets**

- `GE_Cost_FireBlast` — Instant, mana cost = 25
- `GE_Cooldown_FireBlast` — HasDuration, 10.0s, tag `Cooldown.Fire.FireBlast`
- `GE_Damage_Fire` — Shared fire damage GE

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

Add to Aura's `startupAbilityDefinitions` (not `lmbAbility` — this is a startup ability).

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
        <property name="ProjectileClass" value="/Game/.../BP_FireBall.BP_FireBall_C"/>
        <property name="Count" value="12"/>
        <property name="Spread" value="360"/>
        <property name="bHoming" value="false"/>
        <property name="bSetReturnToOwner" value="true"/>
      </node>
    </node>
  </graph>
</ability>
```

**Key Diff:** The `bSetReturnToOwner` property on `SpawnProjectiles` is a **new XML property** added in Phase 3. In the traditional GAS path, this logic was baked into the C++ `SpawnFireBalls()` function (`FireBall->ReturnToActor = AvatarActor`). In the data-driven path, it's a configurable XML property.

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
| **Fireball BP reference** | `TSubclassOf<AAuraFireBall>` in C++ defaults | String path in XML `ProjectileClass` property |
| **GE UAssets** | 3 separate GE BPs | 0 — uses shared C++ GEs |
| **File count** | ~5 files (GA + 3 GEs + FireBallBP) | ~1 file (XML) |

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
- `DebuffChance = 0.5`, `DebuffDamage = 2`, `DebuffDuration = 3`, `DebuffFrequency = 0.5` (stun)
- `MaxNumShockTargets = 5`

**Step 3: Create GE UAssets**

- `GE_Cost_Electrocute` — Instant, mana cost = 5
- `GE_Cooldown_Electrocute` — HasDuration, 3.0s, tag `Cooldown.Lightning.Electrocute`
- `GE_Damage_Lightning` — Lightning damage with stun debuff

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
7. At `TickInterval` (0.2s), re-evaluates targets

In the traditional BP approach, this required:
- A custom Beam Actor or Component BP
- Timer-based tick logic in the ability
- Target chaining logic (find nearby enemies, trace to them)
- Niagara system spawning and parameter updates
- Damage application per tick to each target

**Step 5: Register in RoleConfig**

Add to Aura's `startupAbilityDefinitions`.

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
  <montage path="..." eventTag="Event.Montage.Electrocute"/>
  <graph>
    <node class="Sequence">
      <node class="WaitForTargetData"/>
      <node class="FaceTarget"/>
      <node class="PlayMontage"/>
      <node class="WaitForMontageEvent">
        <property name="EventTag" value="Event.Montage.Electrocute"/>
      </node>
      <node class="ElectrocuteBeam"/>
    </node>
  </graph>
</ability>
```

The `ElectrocuteBeam` node is a **new XML node type** added in Phase 3. It channels a lightning beam with chain targets. All the complex logic (beam VFX, tick-based damage, target chaining, Niagara parameter updates) is handled by the C++ `UElectrocuteBeamNode` + `UElectrocuteBeamTask` implementation.

**Key Diff:** The traditional BP required ~500 lines of custom Blueprint logic for the beam channeling. In the data-driven path, it's a single `<node class="ElectrocuteBeam"/>` in the XML.

---

#### Diffs: GAS-Version vs. AuraAbilityGraph

| Aspect | GAS-Version (Traditional) | AuraAbilityGraph (Data-Driven) |
|---|---|---|
| **Ability class** | Blueprint `GA_Electrocute` extending `UElectrocute` | No Blueprint; `UAuraDataAbility` generic |
| **Channeled beam** | ~500 lines of custom BP logic (ticks, chains, VFX) | Single `ElectrocuteBeam` XML node |
| **Beam VFX** | Niagara system + custom BP actor | Handled by `ElectrocuteBeamTask` C++ class |
| **Target chaining** | Manual trace + target selection in BP | Hardcoded in C++ (`MaxChainTargets=5`, `ChainRadius=850`) |
| **Tick-based damage** | Timer + loop in Blueprint | Handled by `ElectrocuteBeamTask::OnTick()` |
| **Stun debuff** | Applied via GE spec in BP | Damage params in XML (debuffChance=0.5, debuffDamage=2, debuffDuration=3) |
| **GE UAssets** | 3 separate GE BPs | 0 — uses shared C++ GEs |
| **File count** | ~7 files (GA + 3 GEs + BeamActor + Niagara + SoundCue) | ~1 file (XML) |
| **Designer changes** | Editable in BP but complex to modify chain logic | Edit XML property values |

---

## Comparison Summary: Traditional GAS vs. AuraAbilityGraph

### Files per Ability

| | Traditional GAS | AuraAbilityGraph | Savings |
|---|---|---|---|
| **FireBolt** | ~6 files | 1 XML | ~83% |
| **FireGun** | ~6 files | 1 XML | ~83% |
| **ArcaneShards** | ~5 files | 1 XML | ~80% |
| **FireBlast** | ~5 files | 1 XML | ~80% |
| **Electrocute** | ~7 files | 1 XML | ~86% |
| **Total (5 abilities)** | ~29 files | 5 XML | ~83% |

### GE UAssets Eliminated

The AuraAbilityGraph approach eliminates **15 GE Blueprint UAssets** (3 per ability × 5 abilities) by using C++ `UGameplayEffect` subclasses:
- `UAuraManaCostGameplayEffect` — shared by all mana-cost abilities
- `UAuraCooldownGameplayEffect` — shared by all cooldown abilities
- `UAuraDamageGameplayEffect` — shared by all damage abilities (with exec calc)

### Operational Differences

| Concern | Traditional GAS | AuraAbilityGraph |
|---|---|---|
| **Edit ability params** | Open Blueprint editor, find the right property | Edit XML text file |
| **Add new ability variant** | Create new GA BP + duplicate GEs + wire new graph | Copy XML, change values |
| **Balance changes** | Edit BP properties → recompile → test | Edit XML → reload in game (or restart) |
| **Designer access** | Needs UE Editor access + BP knowledge | Can edit XML in any text editor |
| **Runtime reload** | Requires recompile/reload | XML re-parsed at load time |
| **Type safety** | Blueprint compile-time checking | XML validated at parse time (manual) |
| **IDE support** | Full UE editor | Text editor + XML schema |
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

Reason: Passives have the wrong archetype (passive GameplayEffect, not action graph), so they cannot be expressed with the current AuraAbilityGraph node types.

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

From the pending plan (verified 2026-07-30):

| # | Issue | Affected Abilities |
|---|---|---|
| H2 | Inconsistent knockback force direction (Direction vs UpVector) in `ApplyDamageNode`, `HitscanTraceNode`, `SpawnProjectileNode` | All |
| M7 | PlayMontage returns Success when no montage | FireBlast (no montage) |
| M9 | Two damage paths with different context setup (`ApplyDamageNode` vs `CauseDamageNode`) | FireBolt, FireGun, ArcaneShards, FireBlast, Electrocute |
| L14 | CooldownDuration doesn't scale from XML | All |
| L21 | Projectiles hardcode `TargetASC=nullptr` | FireBolt (homing projectiles), FireGun |
| L19 | `ApplyDamage` hardcodes `bIsRadialDamage=false` | All |

---

## Phase 4 Remaining Work (Enemy Abilities + Cleanup)

- [ ] Port enemy abilities (`GA_EnemyFireBolt`, `GA_RangedAttack`, `GA_MeleeAttack`, `GA_HitReact`) to XML definitions
- [ ] Remove legacy `GE_Cost_*` / `GE_Cooldown_*` Blueprint assets (dead code for DataAbility path)
- [ ] Remove unused GE Blueprint assets after migration complete
- [ ] Depends on Phase 3 completing first (already done)
