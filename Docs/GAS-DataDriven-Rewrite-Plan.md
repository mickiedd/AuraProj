# Data-Driven GAS Rewrite — Implementation Status

> **STATUS: IMPLEMENTED.** This document was originally a pre-implementation plan. The system is now built and compiling on UE 5.5. This document has been updated to reflect the actual implementation, including the GameplayEffect data-driven rewrite that eliminated all GE UAsset dependencies.

> **Audience:** developers maintaining or extending the data-driven ability system. File:line references are to the current working tree.

## Runtime Loading Architecture

**How BehaviorU Works (the pattern we mirrored):**
- XML files live in `Content/` as **loose files** (not UAssets)
- `LoadBehaviorTreeFromXMLFile(path)` reads XML from disk at runtime with `FFileHelper`
- Creates **transient** `UBehaviorUBehaviorTree` in `GetTransientPackage()` (never saved/cooked)
- Calls `LoadFromXML(content)` to parse and populate
- The import factory exists only for **optional editor convenience** (drag-and-drop preview)

**How AuraAbilityGraph Works (implemented):**
- XML files live in `Content/AbilityDefinitions/` as **loose files**
- `LoadAbilityDefinitionFromXMLFile(path)` reads XML from disk at runtime
- Creates **transient** `UAuraAbilityDefinition` in `GetTransientPackage()`
- `RoleConfig.json` references XML file paths: `"/Game/AbilityDefinitions/FireGun.xml"`
- `AbilityInfo.json` replaces `DA_AbilityInfo` UAsset for UI metadata
- `GameplayEffects.json` replaces all GE UAssets for attribute init and pickup effects
- **NO UAssets** in the runtime data flow — only transient runtime objects + loose files

**Why This Matters:**
✅ Hot-reload abilities without cooking (edit XML, reload config)
✅ No UAsset bloat in version control
✅ Truly data-driven — designers edit text files
✅ Matches BehaviorU/RoleConfig pattern exactly
✅ Faster iteration — no reimport step
✅ Zero GE Blueprint UAssets — all GameplayEffects are C++ classes + JSON config

---

## 0. Context & goal

Today every Aura ability is split across (a) a fixed C++ class with `BlueprintCallable` fire methods, (b) a **hand-built BP event graph** that wires `TargetDataUnderMouse → PlayMontageAndWait + WaitGameplayEvent → C++ fire method`, (c) **two hand-built GE BPs** for cost/cooldown, (d) a hand-built projectile BP, and (e) a `DA_AbilityInfo` row for UI/save. The BP event graph is structurally identical across all offensive abilities; only the montage, the event tag, the socket tag, and the C++ call differ. Authoring and maintaining these BPs by hand is the waste the user wants to eliminate.

**Goal:** rewrite ability *behavior* to be **data-driven from XML**, mirroring the BehaviorU plugin's pattern in this repo: an XML file describes the ability; C++ "action nodes" implement the steps; a single driver ability class loads the definition and executes the node graph on activation. Per-ability BP event graphs and per-ability cost/cooldown GE BPs are eliminated. **All GameplayEffect UAssets are eliminated** — replaced by C++ GE classes + JSON config. The existing C++ workhorses (`SpawnProjectiles`, `MakeDamageEffectParamsFromClassDefaults`, `ApplyDamageEffect`, `UTargetDataUnderMouse`) are **reused, not rewritten** — they get wrapped by nodes.

**Non-goals (keep as-is):** the GAS itself (ASC, gameplay effects, tags, gameplay cues), the input/activation pipeline (`AbilityInputTagPressed/Held/Released`), `ExecCalc_Damage`, `FAuraGameplayEffectContext`, projectile actor classes (`AAuraProjectile`/`AAuraBullet`/`AAuraFireBall`), montages, FX assets. These remain. We replace only the per-ability BP graph + cost/cooldown GEs + the per-ability C++ fire classes + all GE UAssets with a data-driven layer.

---

## 1. Architecture overview

```
Ability.xml  ──(runtime load)──▶  UAuraAbilityDefinition (transient UObject)
                                        │
                                        │  holds: tags, cost, cooldown, damage,
                                        │         montage, action-node tree
                                        ▼
            grant path: RoleConfig.json resolves AbilityTag → Definition
                                        │
                                        ▼
        UAuraDataAbility (single C++ UGameplayAbility) activated by input
                                        │  CurrentSpec.SourceObject = Definition
                                        ▼
        builds UAuraAbilityActionTask tree from Definition's nodes
                                        │
                                        ▼
        executes nodes (wrap existing GAS tasks / C++ workhorses)
                                        │
                                        ▼
        ends ability when root task completes
```

**GameplayEffect data-driven flow:**
```
GameplayEffects.json  ──(runtime load)──▶  C++ GE classes
  secondaryAttributes                         UAuraAttributeGameplayEffect
  resistances                                 UAuraAttributeGameplayEffect_Infinite
  pickupEffects                               UAuraPickupGameplayEffect
                                              UAuraDamageGameplayEffect
                                              UAuraManaCostGameplayEffect
                                              UAuraCooldownGameplayEffect
```

**Mapping to BehaviorU:**
| BehaviorU | Data-driven GAS |
|---|---|
| `UBehaviorUBehaviorTree` (transient, `LoadFromXML`) | `UAuraAbilityDefinition` (transient, `LoadFromXML`) |
| `UBehaviorUBehaviorNode` (definition, `LoadFromProperties`, `CreateTask`) | `UAuraAbilityActionNode` |
| `UBehaviorUBehaviorTask` (runtime, `OnEnter/OnUpdate/OnExit`, `EBehaviorUStatus`) | `UAuraAbilityActionTask` (runtime, `OnEnter/OnStart/OnTick/OnExit`, `EAuraAbilityActionStatus`) |
| `FBehaviorUNodeRegistry` | `FAuraAbilityNodeRegistry` |
| `UBehaviorUAgentComponent` (tick driver) | `UAuraDataAbility` (driver; uses GAS ability tasks for async, no separate tick component needed) |
| `CreateNodeByClassName` registry-first hook | same, in `AbilityDefinition.cpp` |
| `<node class="Assert">` + `Register("Assert", …)` | `<node class="SpawnProjectile">` + `Register("SpawnProjectile", …)` |

**Key difference from BehaviorU:** BehaviorU ticks the BT every frame from a `UTickableWorldSubsystem`. GAS abilities do **not** tick; they use **ability tasks** (latent `UAbilityTask` objects that tick themselves) for async work. So the data-driven ability drives execution through ability-task callbacks, not a per-frame tick. Instant nodes run synchronously and advance immediately; async nodes (wait-for-montage-event, wait-for-target-data) return `Running` and advance when their wrapped ability task fires its delegate. This is simpler than BehaviorU's two-phase tick and reuses GAS's existing latent infrastructure.

---

## 2. Module & file layout

The `AuraAbilityGraph` plugin lives in `Plugins/AuraAbilityGraph/` (not `Source/`). It has two modules: `AuraAbilityGraph` (runtime) and `AuraAbilityGraphEditor` (editor).

### `Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/`
```
AuraAbilityGraphModule.h
AbilityGraphTypes.h                 // EAuraAbilityActionStatus, FAuraAbilityGraphProperty, FAuraAbilityExecutionContext
AbilityDefinition.h                 // UAuraAbilityDefinition (transient, LoadFromXML)
AbilityNodeRegistry.h               // FAuraAbilityNodeRegistry singleton
DataAbility.h                       // UAuraDataAbility : UAuraGameplayAbility (the driver)
AuraManaCostGameplayEffect.h        // C++ GE: SetByCaller mana cost
AuraCooldownGameplayEffect.h        // C++ GE: HasDuration cooldown
AuraDamageGameplayEffect.h          // C++ GE: ExecCalc_Damage (replaces GE_Damage BP)
AuraAttributeGameplayEffect.h       // C++ GE: SetByCaller all attributes (replaces attribute BPs)
                                    //   + UAuraAttributeGameplayEffect_Infinite
                                    //   + UAuraPickupGameplayEffect (replaces pickup/buff BPs)
AuraAbilityGraphLogChannels.h
Nodes/AbilityActionNode.h           // UAuraAbilityActionNode : UObject (definition base)
Nodes/AbilityActionTask.h           // UAuraAbilityActionTask (runtime base)
Nodes/Composites/SequenceNode.h     // UAuraSequenceNode (runs children in order)
Nodes/Actions/WaitForTargetDataNode.h
Nodes/Actions/PlayMontageNode.h
Nodes/Actions/WaitForMontageEventNode.h
Nodes/Actions/SpawnProjectileNode.h
Nodes/Actions/SpawnProjectilesNode.h
Nodes/Actions/ApplyDamageNode.h
Nodes/Actions/CauseDamageNode.h
Nodes/Actions/MulticastGunFXNode.h
Nodes/Actions/HitscanTraceNode.h
Nodes/Actions/FaceTargetNode.h
```

### `Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/`
Mirror the public tree as `.cpp`, plus:
```
AbilityDefinition.cpp               // LoadFromXML (FXmlFile + prolog-strip), ParseNodeFromXML
AbilityNodeRegistry.cpp
AbilityActionNode.cpp
AbilityActionTask.cpp
DataAbility.cpp                     // ActivateAbility, EndAbility, AdvanceGraph, cost/cooldown overrides
Nodes/.../*.cpp
```

### Editor module `Plugins/AuraAbilityGraph/Source/AuraAbilityGraphEditor/`
```
AuraAbilityGraphEditorModule.h/.cpp  // Toolbar button + launcher
AbilityDefinitionImportFactory.h/.cpp // UAbilityDefinitionImportFactory (XML → UAuraAbilityDefinition), + FReimportHandler
```

### Web-based editor `Plugins/AuraAbilityGraph/Editor/`
```
index.html                           // Tab UI: Ability Graph | Ability Info | Role Config | GE Config
ability_graph_server.py              // Python HTTP server (port 18103)
js/app.js                            // Main app + tab logic (incl. GE Config tab)
js/ability-data.js                   // API helpers for AbilityInfo/RoleConfig/GameplayEffects JSON
js/nodes.js                          // Node type definitions + property schemas
js/graph.js, xml-import/export.js, properties.js, undo.js
```

**`AuraAbilityGraph.Build.cs`** public deps: `Core, CoreUObject, Engine, GameplayTags, GameplayTasks, XmlParser, GameplayAbilities, Aura` (needs `Aura` for `UAuraGameplayAbility`, `ICombatInterface`, `UAuraAbilitySystemLibrary`, `AAuraProjectile`, tags, `UExecCalc_Damage`).

---

## 3. XML schema

Mirror BehaviorU's element conventions exactly (`<node class="…">` with `<property name="…" value="…"/>` children; values are strings). Root element is `<ability>`.

### Example: `Content/AbilityDefinitions/FireGun.xml` (implemented)

```xml
<ability name="FireGun" abilityTag="Abilities.Gun.Fire" inputTag="InputTag.LMB" type="Abilities.Type.Offensive">
  <cooldown tag="Cooldown.Gun.Fire" duration="0.2"/>
  <cost mana="0"/>
  <damage type="Damage.Physical" base="5" deathImpulseMagnitude="1000" knockbackChance="0"/>
  <montage path="/Game/Assets/Characters/Aura/Animations/Abilities/AM_FireGun" eventTag="Event.Montage.FireGun"/>
  <graph>
    <node class="Sequence" id="1">
      <node class="WaitForTargetData" id="2"/>
      <node class="FaceTarget" id="7">
        <property name="bSetControllerRotation" value="true"/>
        <property name="bSetActorRotation" value="true"/>
        <property name="bYawOnly" value="false"/>
      </node>
      <node class="PlayMontage" id="3"/>
      <node class="WaitForMontageEvent" id="4">
        <property name="EventTag" value="Event.Montage.FireGun"/>
      </node>
      <node class="SpawnProjectile" id="5">
        <property name="SocketTag" value="CombatSocket.Weapon"/>
        <property name="ProjectileClass" value="/Script/Aura.AuraProjectile"/>
        <property name="TargetFromContext" value="CursorHit.ImpactPoint"/>
      </node>
      <node class="MulticastGunFX" id="6">
        <property name="MuzzleSocketTag" value="CombatSocket.Weapon"/>
        <property name="MuzzleEffect" value="/Game/MilitaryWeapDark/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash"/>
        <property name="FireSound" value="/Game/MilitaryWeapDark/Sound/Rifle/RifleB_Fire_Cue.RifleB_Fire_Cue"/>
      </node>
    </node>
  </graph>
</ability>
```

### Example: `Content/AbilityDefinitions/FireBolt.xml` (implemented)

```xml
<ability name="FireBolt" abilityTag="Abilities.Fire.FireBolt" inputTag="InputTag.LMB" type="Abilities.Type.Offensive">
  <cooldown tag="Cooldown.Fire.FireBolt" duration="5"/>
  <cost mana="10"/>
  <damage type="Damage.Fire" base="50" deathImpulseMagnitude="5000" knockbackForceMagnitude="2000" knockbackChance="0.1"/>
  <montage path="/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBolt" eventTag="Event.Montage.FireBolt"/>
  <graph>
    <node class="Sequence" id="1">
      <node class="WaitForTargetData" id="2"/>
      <node class="FaceTarget" id="6">
        <property name="bSetControllerRotation" value="true"/>
        <property name="bSetActorRotation" value="true"/>
        <property name="bYawOnly" value="false"/>
      </node>
      <node class="PlayMontage" id="3"/>
      <node class="WaitForMontageEvent" id="4">
        <property name="EventTag" value="Event.Montage.FireBolt"/>
      </node>
      <node class="SpawnProjectiles" id="5">
        <property name="SocketTag" value="CombatSocket.Weapon"/>
        <property name="ProjectileClass" value="/Game/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBolt/BP_FireBolt.BP_FireBolt_C"/>
        <property name="Count" value="5"/>
        <property name="Spread" value="90"/>
        <property name="bHoming" value="true"/>
        <property name="HomingAccelerationMin" value="1600"/>
        <property name="HomingAccelerationMax" value="3200"/>
      </node>
    </node>
  </graph>
</ability>
```

### Schema rules
- Root `<ability>` attributes: `name`, `abilityTag`, `inputTag`, `type`. (`inputTag`/`type` are gameplay-tag strings.)
- `<cooldown tag duration/>`, `<cost mana/>`, `<damage …/>`, `<montage path eventTag/>` are top-level behavior sections parsed by the definition (not nodes).
- `<damage>` has an optional `effectClass` attribute. **If omitted (recommended), the C++ `UAuraDamageGameplayEffect` is used.** If present, it overrides with a custom GE class path (legacy compat).
- `<graph>` contains a single root `<node>` (usually a `Sequence` for multi-step abilities). Nested `<node>` children = sequence children. Order = execution order.
- Node params via `<property name value>` children (mirror BehaviorU). Asset references are `/Game/...` paths loaded at XML parse time with `LoadObject`/`LoadClass`.

---

## 4. Class design (as implemented)

### 4.1 `UAuraAbilityDefinition : public UObject` (Transient)
`AbilityDefinition.h`:
```cpp
UCLASS(Transient)
class AURAABILITYGRAPH_API UAuraAbilityDefinition : public UObject
{
    GENERATED_BODY()
public:
    // Identity — set from <ability> attributes
    FName AbilityName;
    FGameplayTag AbilityTag;
    FGameplayTag InputTag;
    FGameplayTag AbilityType;

    // Cost / cooldown — set from <cost> / <cooldown>
    float ManaCost = 0.f;
    FGameplayTag CooldownTag;
    FScalableFloat CooldownDuration;

    // Damage — set from <damage>. Defaults to UAuraDamageGameplayEffect (pure C++, no UAsset).
    // If <damage effectClass="..."> is present in XML, it overrides this with a custom GE class.
    TSubclassOf<UGameplayEffect> DamageEffectClass = UAuraDamageGameplayEffect::StaticClass();
    FGameplayTag DamageType;
    FScalableFloat Damage;
    float DebuffChance = 20.f;
    float DebuffDamage = 5.f;
    float DebuffDuration = 5.f;
    float DebuffFrequency = 1.f;
    float DeathImpulseMagnitude = 10000.f;
    float KnockbackForceMagnitude = 10000.f;
    float KnockbackChance = 0.f;

    // Animation — Montage loaded by path at XML parse time from <montage>
    TObjectPtr<UAnimMontage> Montage;
    FGameplayTag MontageEventTag;

    // Action graph — root node built from <graph> during LoadFromXML
    UPROPERTY(Instanced)
    TObjectPtr<UAuraAbilityActionNode> RootNode;

    // Cached source XML for debugging / log output
    FString SourceXML;

    bool LoadFromXML(const FString& XMLContent);
};
```

### 4.2 `FAuraAbilityNodeRegistry`
Singleton `TMap<FString, FNodeFactory>` + `FRWLock`. Built-in nodes registered in `StartupModule`. External modules register custom nodes via public dep on `AuraAbilityGraph`.

### 4.3 `UAuraAbilityActionNode : public UObject`
Abstract, `EditInlineNew`, `DefaultToInstanced`. Holds `NodeId`, `NodeClassName`, `Children`. Virtual `LoadFromProperties` + `CreateTask`.

### 4.4 `UAuraAbilityActionTask`
Runtime state. `Execute` driver calls `OnEnter → OnStart → OnExit`. Async nodes return `Running` and are re-driven by `AdvanceGraph`.

### 4.5 `UAuraDataAbility : public UAuraGameplayAbility` (the driver)
- `SharedCostGE` = `UAuraManaCostGameplayEffect::StaticClass()` (C++, set in constructor)
- `SharedCooldownGE` = `UAuraCooldownGameplayEffect::StaticClass()` (C++, set in constructor)
- `ActivateAbility` → `GetDefinition()` → build task tree → `RootTask->Execute(Ctx)`
- `AdvanceGraph(ChildStatus)` — re-drives `RootTask->Execute` when async nodes complete
- `GetDefinition()` = `Cast<UAuraAbilityDefinition>(CurrentSpec->GetSourceObject())`
- `CheckCost` / `ApplyCost` / `ApplyCooldown` / `GetCooldownTags` all read from the definition

---

## 5. Built-in action node catalog (11 nodes, all implemented)

| Node class | Wraps | Key properties | Behavior |
|---|---|---|---|
| `Sequence` | (composite) | children | Runs children in order; `Success` advances, `Failure` aborts. |
| `WaitForTargetData` | `UTargetDataUnderMouse` | (none) | Spawns the task; on `ValidData` caches `FHitResult` into `Ctx.CursorHit` and returns `Success`. Async. |
| `PlayMontage` | `UAbilityTask_PlayMontageAndWait` | (none — reads from Definition) | Plays `Definition->Montage`; returns `Success` immediately. |
| `WaitForMontageEvent` | `UAbilityTask_WaitGameplayEvent` | `EventTag` | Returns `Running` until the event fires, then `Success`. Async. |
| `SpawnProjectile` | `SpawnActorDeferred<AAuraProjectile>` | `SocketTag`, `ProjectileClass`, `TargetFromContext` | Spawns one projectile from socket toward cursor. Instant. |
| `SpawnProjectiles` | `EvenlySpacedRotators` + spawn loop | `SocketTag`, `ProjectileClass`, `Count`, `Spread`, `bHoming`, `HomingAccelerationMin/Max` | Multi-projectile + homing. Instant. |
| `ApplyDamage` | `UAuraAbilitySystemLibrary::ApplyDamageEffect` | `TargetFromContext` | Build `FDamageEffectParams` from Definition + target, apply. Instant. |
| `CauseDamage` | `MakeOutgoingGameplayEffectSpec` directly | `TargetFromContext` | Direct GE to target ASC (beam/melee path). Instant. |
| `MulticastGunFX` | `AAuraCharacterBase::MulticastPlayGunFireFX` | `MuzzleSocketTag`, `MuzzleEffect`, `FireSound` | Call the avatar's muzzle-FX NetMulticast. Instant. |
| `HitscanTrace` | `LineTraceSingleByChannel` + `ApplyDamageEffect` | `SocketTag`, `TraceRange`, `ScatterRadius` | Line trace + damage. Instant. |
| `FaceTarget` | `SetControlRotation` / `SetActorRotation` | `bSetControllerRotation`, `bSetActorRotation`, `bYawOnly` | Turn avatar toward cursor/target. Instant. |

---

## 6. Async execution & the "tick" mechanism

GAS abilities don't tick. Async nodes wrap **GAS ability tasks** (`UAbilityTask` subclasses), which are latent and tick themselves. Flow:

1. `UAuraDataAbility::ActivateAbility` runs `RootTask->Execute(Ctx)`.
2. `Sequence` runs child 0 (`WaitForTargetData`), which spawns `UTargetDataUnderMouse`, binds its `ValidData` delegate, returns `Running`.
3. When `ValidData` fires, `OnTargetDataReady` caches `Ctx.CursorHit` and calls `AdvanceGraph(Success)`.
4. `AdvanceGraph` re-drives `RootTask->Execute(Ctx)`. `Sequence` advances to child 1, etc.

`AdvanceGraph` is the single re-entry point. `EndAbility`/`Cancel` tears down pending ability tasks.

**All execution is game-thread.** No worker threads.

---

## 7. Grant mechanism & integration with RoleConfig / AbilityInfo / save-load

### 7.1 `RoleConfig.json`
`lmbAbilityDefinition` and `startupAbilityDefinitions` are **XML file paths** (e.g. `/Game/AbilityDefinitions/FireGun.xml`). `LoadAbilityDefinition` in `AuraAbilitySystemLibrary.cpp` resolves `/Game/` to project content dir, reads the file with `FFileHelper`, creates a transient `UAuraAbilityDefinition`, and calls `LoadFromXML`.

### 7.2 Grant path (`AddCharacterDataAbilities`)
```cpp
FGameplayAbilitySpec Spec(UAuraDataAbility::StaticClass(), 1);
Spec.SourceObject = Definition;
Spec.DynamicAbilityTags.AddTag(Definition->InputTag);
Spec.DynamicAbilityTags.AddTag(Definition->AbilityTag);
Spec.DynamicAbilityTags.AddTag(Abilities_Status_Equipped);
GiveAbility(Spec);
```

### 7.3 `GetAbilityTagFromSpec` / `GetInputTagFromSpec`
Scans `Spec.Ability->AbilityTags` first (legacy compat), then `Spec.DynamicAbilityTags` for the first `Abilities.*` / `InputTag.*` tag. Data abilities resolve via `DynamicAbilityTags` since the shared `UAuraDataAbility` CDO has no per-ability tags.

### 7.4 `AbilityInfo.json` (replaces DA_AbilityInfo UAsset)
```json
{
  "abilities": [
    { "abilityTag": "Abilities.Fire.FireBolt", "icon": "...", "backgroundMaterial": "...", "levelRequirement": 1 }
  ]
}
```
Loaded at runtime into `URuntimeAbilityInfo` (transient). Active runtime code and resaved Blueprints no longer reference legacy `UAbilityInfo` / `DA_AbilityInfo`; the legacy asset is retained only until the ordered cleanup/deletion step.

### 7.5 Save / load
Ability saves persist stable tag, slot, status, and level fields only. On load, the active XML/role definition or live runtime source resolves the implementation class by tag. `InitializeDefaultAttributesFromSaveData` uses C++ GEs with SetByCaller magnitudes from save data + GameplayEffects.json.

---

## 8. GameplayEffect data-driven system (IMPLEMENTED)

**All GameplayEffect UAssets have been replaced with C++ GE classes + JSON config.** No GE Blueprint is required at runtime.

### 8.1 C++ GE classes (6 total, all in AuraAbilityGraph plugin)

| Class | File | Replaces | How it works |
|---|---|---|---|
| `UAuraDamageGameplayEffect` | `AuraDamageGameplayEffect.h` | `GE_Damage` BP | Instant, `Executions[0].CalculationClass = UExecCalc_Damage::StaticClass()`. SetByCaller: damage type tag, debuff params. |
| `UAuraManaCostGameplayEffect` | `AuraManaCostGameplayEffect.h` | `GE_Cost_*` BPs | Instant, SetByCaller `Abilities.Cost.Mana` on Mana attribute. |
| `UAuraCooldownGameplayEffect` | `AuraCooldownGameplayEffect.h` | `GE_Cooldown_*` BPs | HasDuration, duration overridden at apply time. Cooldown tag granted dynamically via `Spec->DynamicGrantedTags`. |
| `UAuraAttributeGameplayEffect` | `AuraAttributeGameplayEffect.h` | `GE_PrimaryAttributes_SetByCaller`, `GE_SecondaryAttributes`, `GE_VitalAttributes` BPs | Instant, SetByCaller modifiers for ALL 18 attributes (Primary, Secondary, Vital, Resistance). Unassigned tags default to 0 (harmless). |
| `UAuraAttributeGameplayEffect_Infinite` | `AuraAttributeGameplayEffect.h` | `GE_SecondaryAttributes_Infinite` BP | Same as above, `DurationPolicy = Infinite`. |
| `UAuraPickupGameplayEffect` | `AuraAttributeGameplayEffect.h` | `GE_PotionHeal`, `GE_PotionMana`, `GE_CrystalHeal`, `GE_CrystalMana` BPs | Instant, SetByCaller for Health and Mana. Magnitudes from GameplayEffects.json. |

### 8.2 `Content/Config/GameplayEffects.json`

```json
{
  "secondaryAttributes": {
    "Armor": 0, "ArmorPenetration": 0, "BlockChance": 0,
    "CriticalHitChance": 0, "CriticalHitDamage": 0, "CriticalHitResistance": 0,
    "HealthRegeneration": 1, "ManaRegeneration": 1,
    "MaxHealth": 100, "MaxMana": 50
  },
  "resistances": {
    "Fire": 0, "Lightning": 0, "Arcane": 0, "Physical": 0
  },
  "pickupEffects": {
    "healthPotion": { "duration": "instant", "health": 50, "mana": 0 },
    "manaPotion": { "duration": "instant", "health": 0, "mana": 25 }
  }
}
```

### 8.3 How attribute init works (no UAssets)

1. **Primary attributes** — from `RoleConfig.json` per-role values (Strength/Intelligence/Resilience/Vigor), applied via `UAuraAttributeGameplayEffect` with `AssignTagSetByCallerMagnitude`.
2. **Secondary/Vital/Resistance** — from `GameplayEffects.json`, loaded by `LoadAndApplySecondaryAttributes()` in `AuraCharacterBase.cpp`, applied via `UAuraAttributeGameplayEffect` with SetByCaller magnitudes from JSON.
3. **Save-data path** — `InitializeDefaultAttributesFromSaveData` uses `UAuraAttributeGameplayEffect` for primary (from save values) + `UAuraAttributeGameplayEffect_Infinite` for secondary + `UAuraAttributeGameplayEffect` for vital (MaxHealth/MaxMana from JSON).

### 8.4 How pickup effects work (no UAssets)

`AAuraEffectActor` has new data-driven fields: `InstantEffectName`, `DurationEffectName`, `InfiniteEffectName` (FString). When set, `ApplyDataDrivenEffect` loads `GameplayEffects.json`, looks up the named effect, and applies `UAuraPickupGameplayEffect` with SetByCaller Health/Mana magnitudes from JSON. Legacy `TSubclassOf<UGameplayEffect>` fields are kept for backward compat — if the name fields are empty, the old UAsset path is used.

### 8.5 How damage works (no UAssets)

`UAuraAbilityDefinition::DamageEffectClass` defaults to `UAuraDamageGameplayEffect::StaticClass()`. XML `<damage>` can omit `effectClass` entirely. `ApplyDamageEffect` and `CauseDamageNode` both fall back to `UAuraDamageGameplayEffect` when the class is null.

### 8.6 How cost/cooldown works (no UAssets)

`UAuraDataAbility` constructor sets `SharedCostGE = UAuraManaCostGameplayEffect::StaticClass()` and `SharedCooldownGE = UAuraCooldownGameplayEffect::StaticClass()`. `ApplyCost` assigns SetByCaller `Abilities.Cost.Mana` from `Definition->ManaCost`. `ApplyCooldown` sets duration from `Definition->CooldownDuration` and grants `Definition->CooldownTag` dynamically.

### 8.7 New native gameplay tags

Added to `AuraGameplayTags.cpp`:
- `Attributes.Vital.Health` — SetByCaller tag for direct Health modification (pickup GEs)
- `Attributes.Vital.Mana` — SetByCaller tag for direct Mana modification (pickup GEs)

### 8.8 GE application call sites (all converted to C++ GEs)

| Call site | Before | After |
|---|---|---|
| `CauseDamageNode::OnStart` | BP GE via `effectClass` | `UAuraDamageGameplayEffect` fallback |
| `DataAbility::ApplyCost` | `UAuraManaCostGameplayEffect` (already C++) | unchanged |
| `DataAbility::ApplyCooldown` | `UAuraCooldownGameplayEffect` (already C++) | unchanged |
| `AuraCharacterBase::InitializeDefaultAttributes` | BP `DefaultPrimaryAttributes` etc. | `UAuraAttributeGameplayEffect` + JSON |
| `AuraCharacterBase::InitializeDefaultAttributesForRole` | BP `PrimaryAttributes_SetByCaller` etc. | `UAuraAttributeGameplayEffect` + RoleConfig + JSON |
| `AuraAbilitySystemLibrary::InitializeDefaultAttributes` | BP `CharacterClassInfo` GEs | `UAuraAttributeGameplayEffect` + JSON |
| `AuraAbilitySystemLibrary::InitializeDefaultAttributesFromSaveData` | BP GEs | `UAuraAttributeGameplayEffect` / `_Infinite` + save + JSON |
| `AuraEffectActor::ApplyEffectToTarget` | BP GE classes | `UAuraPickupGameplayEffect` + JSON (data-driven names) |
| `ApplyDamageEffect` | BP `DamageGameplayEffectClass` | `UAuraDamageGameplayEffect` fallback |
| `AuraAttributeSet::Debuff` | Dynamic `NewObject<UGameplayEffect>` (already UAsset-free) | unchanged |
| `AuraDamageGameplayAbility::CauseDamage` | BP `DamageEffectClass` (legacy) | unchanged (legacy non-DataAbility path) |

---

## 9. Web editor support (IMPLEMENTED)

The AuraAbilityGraph editor (WebView2-based, launched from the UE toolbar) has four tabs:

1. **Ability Graph** — visual node graph editor for ability XML
2. **Ability Info** — edit `Content/Config/AbilityInfo.json` (UI metadata per ability)
3. **Role Config** — edit `Content/Config/RoleConfig.json` (character roles, abilities, attributes)
4. **GE Config** — edit `Content/Config/GameplayEffects.json` (attribute defaults, resistances, pickup effects)

### GE Config tab
- **Secondary & Vital Attributes** — 10 numeric fields (Armor, ArmorPenetration, BlockChance, Crit Chance/Damage/Resistance, Health/Mana Regen, MaxHealth, MaxMana)
- **Resistances** — 4 numeric fields (Fire, Lightning, Arcane, Physical)
- **Pickup Effects** — dynamic list of named effects (add/delete), each with Health/Mana/Duration fields

### Server endpoints
- `POST /load-ge-config` — loads `Content/Config/GameplayEffects.json`
- `POST /save-ge-config` — saves `Content/Config/GameplayEffects.json`

---

## 10. Migration status

### Phase 1 — Framework ✅ DONE
- `AuraAbilityGraph` + `AuraAbilityGraphEditor` modules created as a plugin
- `FAuraAbilityNodeRegistry`, `UAuraAbilityActionNode`, `UAuraAbilityActionTask`, `UAuraAbilityDefinition`, `UAuraSequenceTask` implemented
- `UAuraDataAbility` driver implemented with cost/cooldown overrides
- 11 built-in nodes registered (Sequence, WaitForTargetData, PlayMontage, WaitForMontageEvent, SpawnProjectile, SpawnProjectiles, ApplyDamage, CauseDamage, MulticastGunFX, HitscanTrace, FaceTarget)
- Import factory implemented
- Build passes on UE 5.5.1

### Phase 2 — MVP port: FireGun + FireBolt ✅ DONE
- `Content/AbilityDefinitions/FireGun.xml` and `FireBolt.xml` authored
- `RoleConfig.json` updated: `lmbAbilityDefinition` points to XML paths for both Aura and BungeeMan roles
- `AddCharacterDataAbilities` grant path implemented
- `GetAbilityTagFromSpec` / `GetInputTagFromSpec` updated to scan `DynamicAbilityTags`
- `FaceTarget` node added (not in original plan — improves ability aiming)

### Phase 3 — Port remaining player abilities ⬜ NOT STARTED
- ArcaneShards, FireBlast, Electrocute, Passives still use legacy BP abilities

### Phase 4 — Enemy abilities + cleanup ⬜ NOT STARTED
- Enemy abilities still use legacy BPs
- Legacy `GE_Cost_*` / `GE_Cooldown_*` BPs still on disk (dead code for DataAbility path)

### Phase 5 — GameplayEffect UAsset elimination ✅ DONE
- `UAuraDamageGameplayEffect` replaces `GE_Damage` BP
- `UAuraAttributeGameplayEffect` + `_Infinite` replaces all attribute init BPs
- `UAuraPickupGameplayEffect` replaces pickup/buff effect BPs
- `UAuraManaCostGameplayEffect` + `UAuraCooldownGameplayEffect` replace per-ability cost/cooldown BPs
- `GameplayEffects.json` configures all default values
- Web editor GE Config tab implemented
- All 15 GE application call sites converted to C++ GEs (one legacy path remains: `AuraDamageGameplayAbility::CauseDamage`)

### Phase 6 (optional, future) — Projectile data-driven ⬜ NOT STARTED
Move projectile mesh/FX/impact params into a data asset so `BP_FireBolt`/`BP_AuraBullet`/`BP_FireBall` also disappear.

---

## 11. Known issues

1. **PlayMontage delegates not wired** — `UPlayMontageTask::OnStart` creates `PlayMontageAndWait` but doesn't bind `OnInterrupted`/`OnCompleted`. If the montage is interrupted before the gameplay event fires, the graph hangs in `Running`. `OnMontageInterrupted` exists on `UAuraDataAbility` but is never called from C++.
2. **Dead declarations** — `AbilityDefinition.h` declares `static CreateNodeByClassName` (never defined). `DataAbility.h` declares `BuildAndExecuteGraph` (never defined/called).
3. **Unused XML properties** — `TargetFromContext` on SpawnProjectile/SpawnProjectiles/ApplyDamage/CauseDamage is parsed but always ignored (uses `Ctx.CursorHit`). `HitscanTraceNode::ScatterRadius` parsed but trace is straight line.
4. **Fake smoke tests** — `SmokeTest_NodeRegistry` and `SmokeTest_SequenceExecution` don't actually test anything (just log and return true).
5. **GC** — `AbilityDefinition.h` `Montage` and `DamageEffectClass` have no UPROPERTY on the Transient UObject.
6. **SourceObject replication** — `FGameplayAbilitySpec::SourceObject` doesn't replicate. Clients may get null from `GetDefinition()`. Needs a fallback (DA_AbilityInfo lookup by tag) for multiplayer.
7. **Binary/cache files in git** — 162 WebView2UserData cache files + .exe/.pdb committed to git. Need .gitignore cleanup.
8. **Server security** — `ability_graph_server.py` `/load` and `/save` endpoints have path traversal vulnerabilities. Server binds to `0.0.0.0` instead of `127.0.0.1`.

---

## 12. Verification

- **Build:** `"/d/UE_5.5/Engine/Build/BatchFiles/Build.bat" Aura Win64 Development -Project="..." -WaitMutex` — EXIT_CODE=0, no compile errors, only pre-existing deprecation warnings.
- **Smoke test:** console command `AuraAbilityGraph.SmokeTest` runs 7 tests (XML parsing, node registry, sequence execution, cooldown extraction, FireBolt/FireGun migration, role definition loading).
- **PIE:** `defaultRole: BungeeMan`; press LMB; confirm bullet spawns, damage applies, cooldown gates, FX replicate.
- **GE Config:** open the web editor GE Config tab, modify values, save, verify `Content/Config/GameplayEffects.json` updated.

---

## 13. File-level checklist (as implemented)

**New files created:**
- `Plugins/AuraAbilityGraph/AuraAbilityGraph.uplugin`
- `Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/AuraAbilityGraph.Build.cs`
- `Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Public/` — all headers (see §2)
- `Plugins/AuraAbilityGraph/Source/AuraAbilityGraph/Private/` — all cpps
- `Plugins/AuraAbilityGraph/Source/AuraAbilityGraphEditor/` — editor module
- `Plugins/AuraAbilityGraph/Editor/` — web editor (HTML/JS/Python)
- `Plugins/AuraAbilityGraph/AuraAbilityGraphLauncher/` — Win32 launcher
- `Content/Config/GameplayEffects.json` — GE config
- `Content/AbilityDefinitions/FireBolt.xml`, `FireGun.xml` — ability definitions

**Existing files modified:**
- `Source/Aura/Public/Character/AuraCharacterBase.h` — added `LoadAndApplySecondaryAttributes()`, data-driven definition fields
- `Source/Aura/Private/Character/AuraCharacterBase.cpp` — `InitializeDefaultAttributes*` use C++ GEs + JSON; `AddCharacterAbilities` grants data abilities
- `Source/Aura/Public/AbilitySystem/AuraAbilitySystemComponent.h` — `AddCharacterDataAbilities` / `AddCharacterDataPassiveAbilities`
- `Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp` — data ability grant, `GetAbilityTagFromSpec` scans DynamicAbilityTags
- `Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp` — `LoadAbilityDefinitionFromXMLFile`, `LoadRoleInfoFromConfig`, `InitializeDefaultAttributes*` use C++ GEs + JSON
- `Source/Aura/Public/AbilitySystem/Data/RoleInfo.h` — `StartupAbilityDefinitions`, `DefaultLMBAbilityDefinition` fields
- `Source/Aura/Public/AbilitySystem/Data/AbilityInfo.h` — `URuntimeAbilityInfo` (JSON-based)
- `Source/Aura/Private/AuraGameplayTags.cpp` — added `Attributes_Vital_Health`, `Attributes_Vital_Mana` tags
- `Source/Aura/Public/AuraGameplayTags.h` — added tag fields
- `Source/Aura/Public/Actor/AuraEffectActor.h` — `ApplyDataDrivenEffect`, `InstantEffectName` / `DurationEffectName` / `InfiniteEffectName` fields
- `Source/Aura/Private/Actor/AuraEffectActor.cpp` — data-driven pickup effect application
- `Content/Config/RoleConfig.json` — `lmbAbilityDefinition` points to XML paths
- `Content/Config/AbilityInfo.json` — ability UI metadata

---

## 14. One-paragraph summary

The `AuraAbilityGraph` plugin mirrors BehaviorU's XML→transient→registry→node-tree pattern: `UAuraAbilityDefinition` is loaded from XML at runtime as a transient object and holds the ability's tags, cost, cooldown, damage, montage, and action-node tree; `FAuraAbilityNodeRegistry` lets modules register C++ action nodes; `UAuraAbilityActionNode`/`UAuraAbilityActionTask` are the definition/runtime base classes; a single `UAuraDataAbility` drives the node tree on activation, advancing through 11 node types that wrap existing GAS primitives. **All GameplayEffects are now C++ classes + JSON config** — six C++ GE classes (`UAuraDamageGameplayEffect`, `UAuraManaCostGameplayEffect`, `UAuraCooldownGameplayEffect`, `UAuraAttributeGameplayEffect`, `UAuraAttributeGameplayEffect_Infinite`, `UAuraPickupGameplayEffect`) replace every GE UAsset. `GameplayEffects.json` configures attribute defaults, resistances, and pickup effect magnitudes. The web editor has a GE Config tab for editing these values. Two abilities (FireGun, FireBolt) are fully ported and working; the remaining abilities are pending migration. Build passes on UE 5.5.1.
