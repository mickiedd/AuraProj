# Data-Driven GAS Rewrite — Implementation Plan

> **CRITICAL CORRECTION (Applied):** This plan originally described importing XML files into UAsset data assets (like typical Unreal assets). This was **incorrect** and violated the BehaviorU pattern being mirrored. The **correct implementation** (now applied) loads XML/JSON files **at runtime from disk** as transient objects, exactly like BehaviorU does. See "Runtime Loading Architecture" section below for details.

> **Audience:** the LLM agent that will implement this. Read top to bottom before coding. File:line references are to the current working tree. When a reference says "mirror X", open X and copy its shape.

## Runtime Loading Architecture (CORRECTED)

**How BehaviorU Actually Works (and what we now mirror):**
- XML files live in `Content/` as **loose files** (not UAssets)
- `LoadBehaviorTreeFromXMLFile(path)` reads XML from disk at runtime with `FFileHelper`
- Creates **transient** `UBehaviorUBehaviorTree` in `GetTransientPackage()` (never saved/cooked)
- Calls `LoadFromXML(content)` to parse and populate
- The import factory exists only for **optional editor convenience** (drag-and-drop preview)

**How AuraAbilityGraph Now Works (corrected implementation):**
- XML files live in `Content/AbilityDefinitions/` as **loose files**
- `LoadAbilityDefinitionFromXMLFile(path)` reads XML from disk at runtime
- Creates **transient** `UAuraAbilityDefinition` in `GetTransientPackage()`
- `RoleConfig.json` references XML file paths: `"/Game/AbilityDefinitions/FireGun.xml"`
- `AbilityInfo.json` replaces `DA_AbilityInfo` UAsset for UI metadata
- **NO UAssets** in the runtime data flow—only transient runtime objects

**Why This Matters:**
✅ Hot-reload abilities without cooking (edit XML, reload config)  
✅ No UAsset bloat in version control  
✅ Truly data-driven—designers edit text files  
✅ Matches BehaviorU/RoleConfig pattern exactly  
✅ Faster iteration—no reimport step

---

## 0. Context & goal

Today every Aura ability is split across (a) a fixed C++ class with `BlueprintCallable` fire methods, (b) a **hand-built BP event graph** that wires `TargetDataUnderMouse → PlayMontageAndWait + WaitGameplayEvent → C++ fire method`, (c) **two hand-built GE BPs** for cost/cooldown, (d) a hand-built projectile BP, and (e) a `DA_AbilityInfo` row for UI/save. The BP event graph is structurally identical across all offensive abilities; only the montage, the event tag, the socket tag, and the C++ call differ. Authoring and maintaining these BPs by hand is the waste the user wants to eliminate.

**Goal:** rewrite ability *behavior* to be **data-driven from XML**, mirroring the BehaviorU plugin's pattern in this repo: an XML file describes the ability; C++ "action nodes" implement the steps; a single driver ability class loads the definition and executes the node graph on activation. Per-ability BP event graphs and per-ability cost/cooldown GE BPs are eliminated. The existing C++ workhorses (`SpawnProjectiles`, `MakeDamageEffectParamsFromClassDefaults`, `ApplyDamageEffect`, `UTargetDataUnderMouse`) are **reused, not rewritten** — they get wrapped by nodes.

**Non-goals (keep as-is):** the GAS itself (ASC, gameplay effects, tags, gameplay cues), the input/activation pipeline (`AbilityInputTagPressed/Held/Released`), `ExecCalc_Damage`, `FAuraGameplayEffectContext`, projectile actor classes (`AAuraProjectile`/`AAuraBullet`/`AAuraFireBall`), montages, FX assets. These remain. We replace only the per-ability BP graph + cost/cooldown GEs + the per-ability C++ fire classes with a data-driven layer.

**BehaviorU reference files to mirror (load-bearing):**
- Loader: `Plugins/BehaviorU/Source/BehaviorURuntime/Private/BehaviorTree/BehaviorUBehaviorTree.cpp` (`LoadFromXML` at `:195`, `ParseNodeFromXML` at `:90`, `CreateNodeByClassName` at `:26` — the registry-first dispatch hook).
- Registry: `Plugins/BehaviorU/Source/BehaviorURuntime/Public/BehaviorUNodeRegistry.h` + `.cpp` (singleton `TMap<FString,FNodeFactory>` + `FRWLock`, `Register`/`Create`).
- Node base: `Plugins/BehaviorU/Source/BehaviorURuntime/Public/BehaviorTree/BehaviorUBehaviorNode.h` (definition; `EditInlineNew`/`DefaultToInstanced`, `LoadFromProperties`, `CreateTask`).
- Task base: `Plugins/BehaviorU/Source/BehaviorURuntime/Public/BehaviorTree/BehaviorUBehaviorTask.h` + `.cpp` (runtime state; `Execute`/`OnEnter`/`OnUpdate`/`OnExit`, `EBehaviorUStatus`).
- External node template: `Plugins/AuraAutoTest/Source/AuraAutoTestRuntime/Public/AutoTestNodes.h` + `Private/AutoTestNodes.cpp` + `Private/AutoTestRuntimeModule.cpp:11-24` (registration in `StartupModule`).
- Import factory: `Plugins/BehaviorU/Source/BehaviorUEditor/Private/BehaviorUBehaviorTreeFactory.cpp` (XML → `UDataAsset` import + reimport).

**Aura GAS reference files (the surface area to integrate with):**
- Ability base chain: `Source/Aura/Public/AbilitySystem/Abilities/AuraGameplayAbility.h`, `AuraDamageGameplayAbility.h/.cpp`, `AuraProjectileSpell.h/.cpp`, `AuraFireBolt.h/.cpp`, `AuraFireGun.h/.cpp`.
- Damage pipeline: `Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp:823` (`ApplyDamageEffect`), `AuraDamageGameplayAbility.cpp:18` (`MakeDamageEffectParamsFromClassDefaults`).
- Target data: `Source/Aura/Private/AbilitySystem/AbilityTasks/TargetDataUnderMouse.cpp` (`SendMouseCursorData` `:44`, `OnTargetDataReplicatedCallback` `:102`).
- Grant: `Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp:78` (`AddCharacterAbilities`), `:47` (`AddCharacterAbilitiesFromSaveData`), `:271` (`GetAbilityTagFromSpec`).
- Role hook: `Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp` (`LoadRoleConfig` ~`:420`), `Source/Aura/Private/Character/AuraCharacterBase.cpp:155` (`ApplyRole`, LMB strip `:178-190`), `Source/Aura/Public/AbilitySystem/Data/RoleInfo.h` (`FRoleDefaultInfo`).
- Tags: `Source/Aura/Private/AuraGameplayTags.cpp` (`InitializeNativeGameplayTags` `:9`).
- Ability info / save: `Source/Aura/Public/AbilitySystem/Data/AbilityInfo.h` (`FAuraAbilityInfo`), `Source/Aura/Public/Game/LoadScreenSaveGame.h:52` (`FSavedAbility`), `Source/Aura/Private/Character/AuraCharacter.cpp:400` (`SaveProgress_Implementation`).

---

## 1. Architecture overview

```
Ability.xml  ──(import factory)──▶  UAuraAbilityDefinition (UDataAsset, cooked)
                                          │
                                          │  holds: tags, cost, cooldown, damage,
                                          │         montage, action-node tree
                                          ▼
              grant path: RoleConfig / DA_AbilityInfo resolves AbilityTag → Definition
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

**Mapping to BehaviorU:**
| BehaviorU | Data-driven GAS |
|---|---|
| `UBehaviorUBehaviorTree` (UDataAsset, `LoadFromXML`) | `UAuraAbilityDefinition` (UDataAsset, `LoadFromXML`) |
| `UBehaviorUBehaviorNode` (definition, `LoadFromProperties`, `CreateTask`) | `UAuraAbilityActionNode` |
| `UBehaviorUBehaviorTask` (runtime, `OnEnter/OnUpdate/OnExit`, `EBehaviorUStatus`) | `UAuraAbilityActionTask` (runtime, `OnEnter/OnStart/OnTick/OnExit`, `EAuraAbilityActionStatus`) |
| `FBehaviorUNodeRegistry` | `FAuraAbilityNodeRegistry` |
| `UBehaviorUAgentComponent` (tick driver) | `UAuraDataAbility` (driver; uses GAS ability tasks for async, no separate tick component needed) |
| `CreateNodeByClassName` registry-first hook | same, in `UAuraAbilityDefinition::CreateNodeByClassName` |
| `<node class="Assert">` + `Register("Assert", …)` | `<node class="SpawnProjectile">` + `Register("SpawnProjectile", …)` |

**Key difference from BehaviorU:** BehaviorU ticks the BT every frame from a `UTickableWorldSubsystem`. GAS abilities do **not** tick; they use **ability tasks** (latent `UAbilityTask` objects that tick themselves) for async work. So the data-driven ability drives execution through ability-task callbacks, not a per-frame tick. Instant nodes run synchronously and advance immediately; async nodes (wait-for-montage-event, wait-for-target-data) return `Running` and advance when their wrapped ability task fires its delegate. This is simpler than BehaviorU's two-phase tick and reuses GAS's existing latent infrastructure.

---

## 2. Module & file layout

Create a **new runtime module** `AuraAbilityGraph` (and an editor module for the import factory) inside the Aura project (not a plugin — keep it in `Source/`). Add to `Aura.uproject` and create `Source/AuraAbilityGraph/AuraAbilityGraph.Build.cs`.

> Rationale: a separate module lets `AuraAutoTest` (or any future module) register custom ability nodes by depending on `AuraAbilityGraph`, exactly like modules depend on `BehaviorURuntime`. It also keeps the new system cohesive.

### `Source/AuraAbilityGraph/Public/`
```
AuraAbilityGraphModule.h
AbilityGraphTypes.h                 // EAuraAbilityActionStatus, FAuraAbilityGraphProperty (Name/Value), etc.
AbilityDefinition.h                 // UAuraAbilityDefinition : UDataAsset (LoadFromXML, CreateNodeByClassName)
AbilityNodeRegistry.h               // FAuraAbilityNodeRegistry singleton
Nodes/AbilityActionNode.h           // UAuraAbilityActionNode : UObject (definition base)
Nodes/AbilityActionTask.h           // UAuraAbilityActionTask (runtime base; holds UAbilitySystemComponent*, FGameplayAbilitySpecHandle)
Nodes/Composites/SequenceNode.h     // UAuraSequenceNode (runs children in order)
Nodes/Actions/WaitForTargetDataNode.h
Nodes/Actions/PlayMontageNode.h
Nodes/Actions/WaitForMontageEventNode.h
Nodes/Actions/SpawnProjectileNode.h
Nodes/Actions/SpawnProjectilesNode.h
Nodes/Actions/ApplyDamageNode.h
Nodes/Actions/MulticastGunFXNode.h
Nodes/Actions/HitscanTraceNode.h
Nodes/Actions/CauseDamageNode.h
DataAbility.h                       // UAuraDataAbility : UAuraGameplayAbility (the driver)
```

### `Source/AuraAbilityGraph/Private/`
Mirror the public tree as `.cpp`, plus:
```
AbilityDefinition.cpp               // LoadFromXML (FXmlFile + prolog-strip), ParseNodeFromXML, CreateNodeByClassName
AbilityNodeRegistry.cpp
AbilityActionNode.cpp
AbilityActionTask.cpp
Nodes/.../*.cpp
DataAbility.cpp
```

### Editor module `Source/AuraAbilityGraphEditor/`
```
AuraAbilityGraphEditorModule.h/.cpp
AbilityDefinitionImportFactory.h/.cpp   // UAbilityDefinitionImportFactory : UAssetImportFactory (XML → UAuraAbilityDefinition), + FReimportHandler
```

**`AuraAbilityGraph.Build.cs`** public deps: `Core, CoreUObject, Engine, GameplayTags, XmlParser, GameplayAbilities, Aura` (needs `Aura` for `UAuraGameplayAbility`, `ICombatInterface`, `UAuraAbilitySystemLibrary`, `AAuraProjectile`, tags). `XmlParser` is load-bearing (loader). Editor module deps: `UnrealEd, AssetTools, EditorScriptingUtilities` + the runtime module.

---

## 3. XML schema

Mirror BehaviorU's element conventions exactly (`<node class="…">` with `<property name="…" value="…"/>` children; values are strings; `Self.`-prefix not needed here; FVectors via `FVector::ToString` format if ever needed). Root element is `<ability>`.

### Example: `FireGun.xml` (the MVP port — replaces `GA_FireGun` BP + `UAuraFireGun` C++)

```xml
<ability version="1" name="FireGun"
         abilityTag="Abilities.Gun.Fire"
         inputTag="InputTag.LMB"
         type="Abilities.Type.Offensive">
  <property name="displayName" value="Fire Gun"/>
  <property name="levelRequirement" value="1"/>

  <cooldown tag="Cooldown.Gun.Fire" duration="0.2"/>
  <cost mana="0"/>                               <!-- 0 = no mana cost (cooldown-only) -->
  <damage effectClass="/Game/Blueprints/AbilitySystem/Aura/Effects/GE_Damage.GE_Damage"
          type="Damage.Physical" base="5"
          deathImpulseMagnitude="1000" knockbackChance="0"/>
  <montage path="/Game/Assets/Characters/Aura/Animations/Abilities/AM_FireGun"
           eventTag="Event.Montage.FireGun"/>

  <graph>
    <node class="WaitForTargetData"/>            <!-- UTargetDataUnderMouse, caches CursorHit on the task ctx -->
    <node class="PlayMontage" montageProperty="montage" eventTagProperty="montage.eventTag"/>
    <node class="WaitForMontageEvent" tag="Event.Montage.FireGun"/>
    <node class="SpawnProjectile"
          socketTag="CombatSocket.Weapon"
          projectileClass="/Game/Blueprints/Actor/BP_AuraBullet.BP_AuraBullet_C"
          targetFromContext="CursorHit.ImpactPoint"/>
    <node class="MulticastGunFX"
          muzzleSocketTag="CombatSocket.Weapon"
          muzzleEffect="/Game/MilitaryWeapDark/FX/P_AssaultRifle_MuzzleFlash.P_AssaultRifle_MuzzleFlash"
          fireSound="/Game/MilitaryWeapDark/Sound/.../sfx_GunFire"/>
  </graph>
</ability>
```

### Example: `FireBolt.xml` (multi-projectile + homing — replaces `GA_FireBolt` + `UAuraFireBolt`)

```xml
<ability version="1" name="FireBolt"
         abilityTag="Abilities.Fire.FireBolt" inputTag="InputTag.LMB"
         type="Abilities.Type.Offensive">
  <cooldown tag="Cooldown.Fire.FireBolt" duration="2"/>
  <cost mana="5"/>
  <damage effectClass="/Game/.../GE_Damage.GE_Damage" type="Damage.Fire" base="10"
          debuffChance="20" debuffDamage="5" debuffDuration="5" debuffFrequency="1"/>
  <montage path="/Game/Assets/Characters/Aura/Animations/Abilities/AM_Cast_FireBolt"
           eventTag="Event.Montage.FireBolt"/>
  <graph>
    <node class="WaitForTargetData"/>
    <node class="PlayMontage"/>
    <node class="WaitForMontageEvent" tag="Event.Montage.FireBolt"/>
    <node class="SpawnProjectiles"
          socketTag="CombatSocket.Weapon"
          projectileClass="/Game/Blueprints/AbilitySystem/Aura/Abilities/Fire/FireBolt/BP_FireBolt.BP_FireBolt_C"
          count="5" spread="90"
          homing="true" homingAccelMin="1600" homingAccelMax="3200"
          targetFromContext="CursorHit"/>
  </graph>
</ability>
```

### Schema rules
- Root `<ability>` attributes: `version`, `name`, `abilityTag`, `inputTag`, `type`. (`inputTag`/`type` are gameplay-tag strings.)
- Metadata via `<property name value>` children of `<ability>` (mirror BehaviorU `<property>` convention).
- `<cooldown tag duration/>`, `<cost mana/>`, `<damage …/>`, `<montage path eventTag/>` are top-level behavior sections parsed by the definition (not nodes).
- `<graph>` contains a single root `<node>` (usually a `Sequence` for multi-step abilities). Nested `<node>` children = sequence children. Order = execution order.
- Node params via `<property name value>` children (mirror BehaviorU). Asset references are `/Game/...` paths; the node's `LoadFromProperties` resolves them with `LoadObject`/`LoadClass`/`FSoftObjectPath` (decide: hard refs loaded at import vs. soft refs loaded at activation — see §10).
- `targetFromContext="CursorHit"` / `"CursorHit.ImpactPoint"`: nodes read shared context from the executing task tree (see §6 — a shared `FAuraAbilityExecutionContext` carrying the cached cursor `FHitResult`, the avatar, the ASC, the spec handle).

---

## 4. Class design

### 4.1 `UAuraAbilityDefinition : public UDataAsset`  (mirror `UBehaviorUBehaviorTree`)
`AbilityDefinition.h`:
```cpp
UCLASS()
class AURAABILITYGRAPH_API UAuraAbilityDefinition : public UDataAsset
{
    GENERATED_BODY()
public:
    // Identity / tags (from <ability> attributes)
    UPROPERTY(VisibleAnywhere, Category="Ability") FName AbilityName;
    UPROPERTY(EditDefaultsOnly, Category="Ability") FGameplayTag AbilityTag;   // Abilities.*
    UPROPERTY(EditDefaultsOnly, Category="Ability") FGameplayTag InputTag;     // InputTag.*
    UPROPERTY(EditDefaultsOnly, Category="Ability") FGameplayTag AbilityType;  // Abilities.Type.*

    // Cost / cooldown (from <cooldown>/<cost>)
    UPROPERTY(EditDefaultsOnly, Category="Cost") float ManaCost = 0.f;         // 0 = none
    UPROPERTY(EditDefaultsOnly, Category="Cooldown") FGameplayTag CooldownTag;
    UPROPERTY(EditDefaultsOnly, Category="Cooldown") FScalableFloat CooldownDuration;
    // Damage defaults (from <damage>) — fed into MakeDamageEffectParamsFromClassDefaults-style builder
    UPROPERTY(EditDefaultsOnly, Category="Damage") TSubclassOf<UGameplayEffect> DamageEffectClass;
    UPROPERTY(EditDefaultsOnly, Category="Damage") FGameplayTag DamageType;
    UPROPERTY(EditDefaultsOnly, Category="Damage") FScalableFloat Damage;
    UPROPERTY(EditDefaultsOnly, Category="Damage") float DebuffChance=20.f, DebuffDamage=5.f, DebuffDuration=5.f, DebuffFrequency=1.f;
    UPROPERTY(EditDefaultsOnly, Category="Damage") float DeathImpulseMagnitude=1000.f, KnockbackForceMagnitude=1000.f, KnockbackChance=0.f;
    // Montage (from <montage>)
    UPROPERTY(EditDefaultsOnly, Category="Anim") TObjectPtr<UAnimMontage> Montage;
    UPROPERTY(EditDefaultsOnly, Category="Anim") FGameplayTag MontageEventTag;

    // The action graph (from <graph>)
    UPROPERTY(Instanced, VisibleAnywhere, Category="Graph") TObjectPtr<UAuraAbilityActionNode> RootNode;

    // XML load (mirror UBehaviorUBehaviorTree::LoadFromXML)
    bool LoadFromXML(const FString& XMLContent);
    FString SourceXML;   // cached for debugging/reimport

    // Node-class dispatch with registry-first hook (mirror CreateNodeByClassName)
    static UAuraAbilityActionNode* CreateNodeByClassName(const FString& ClassName, UObject* Outer);
};
```
`AbilityDefinition.cpp` implements `LoadFromXML` by copying `BehaviorUBehaviorTree.cpp:195-279` (FXmlFile + prolog-strip + newline-after-`>` workaround at `:200-217` — **keep that preprocessing**, it's load-bearing for single-line XML), then `ParseNodeFromXML` (copy `:90-150`), then `CreateNodeByClassName` (copy `:26-88` but with `FAuraAbilityNodeRegistry::Get().Create(...)` first). Parse `<cooldown>/<cost>/<damage>/<montage>` sections into the UPROPERTYs. Parse `<graph>` → `RootNode`.

### 4.2 `FAuraAbilityNodeRegistry`  (mirror `FBehaviorUNodeRegistry` exactly)
`AbilityNodeRegistry.h`:
```cpp
struct FAuraAbilityNodeRegistry
{
    using FNodeFactory = TFunction<UAuraAbilityActionNode*(UObject* Outer)>;
    static FAuraAbilityNodeRegistry& Get();
    void Register(const FString& ClassName, FNodeFactory Factory);   // call in module StartupModule
    UAuraAbilityActionNode* Create(const FString& ClassName, UObject* Outer) const;  // nullptr if unknown
private:
    TMap<FString, FNodeFactory> Factories;
    mutable FRWLock Lock;
};
```
Built-in nodes are registered in `FAuraAbilityGraphModule::StartupModule` (NOT hardwired in `CreateNodeByClassName` — improve on BehaviorU by registering built-ins through the registry too, so the if/else chain is empty). External modules register custom nodes in their own `StartupModule` with a public dep on `AuraAbilityGraph` (mirror `AutoTestRuntimeModule.cpp:11-24`).

### 4.3 `UAuraAbilityActionNode : public UObject`  (mirror `UBehaviorUBehaviorNode`)
`Nodes/AbilityActionNode.h`:
```cpp
UCLASS(Abstract, EditInlineNew, DefaultToInstanced)
class AURAABILITYGRAPH_API UAuraAbilityActionNode : public UObject
{
    GENERATED_BODY()
public:
    UPROPERTY() int32 NodeId;
    UPROPERTY() FString NodeClassName;
    UPROPERTY(Instanced) TArray<UAuraAbilityActionNode*> Children;   // for Sequence

    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) {}
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const { return nullptr; }   // leaf/sequence override
};
```
`FAuraAbilityGraphProperty` = `{ FString Name; FString Value; }` (mirror `FBehaviorUProperty` in `BehaviorUTypes.h:207-222`).

### 4.4 `UAuraAbilityActionTask`  (runtime state; mirror `UBehaviorUBehaviorTask`)
`Nodes/AbilityActionTask.h`:
```cpp
enum class EAuraAbilityActionStatus : uint8 { Invalid, Success, Failure, Running };

UCLASS(Abstract)
class AURAABILITYGRAPH_API UAuraAbilityActionTask : public UObject
{
    GENERATED_BODY()
public:
    UAuraAbilityActionNode* NodeDef = nullptr;          // the definition this task runs
    UAuraDataAbility* OwnerAbility = nullptr;           // the driving ability (for ASC, spec, EndAbility)
    UAuraAbilityActionTask* ParentTask = nullptr;
    TArray<UAuraAbilityActionTask*> ChildTasks;         // for Sequence
    int32 ActiveChildIndex = 0;

    virtual void Init(UAuraAbilityActionNode* InNode, UAuraDataAbility* InOwner);   // builds ChildTasks via Node->CreateTask
    EAuraAbilityActionStatus Execute(FAuraAbilityExecutionContext& Ctx);            // non-virtual driver: OnEnter → OnStart/OnTick → OnExit
    virtual void OnEnter(FAuraAbilityExecutionContext& Ctx) {}
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) { return Success; }  // synchronous leaf result
    virtual EAuraAbilityActionStatus OnTick(FAuraAbilityExecutionContext& Ctx) { return Running; }   // for async leaves returning Running
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) {}
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx);                          // tear down ability tasks / delegates
};
```
**Execution model:** `Execute` calls `OnEnter`, then `OnStart` (synchronous result). If `OnStart` returns `Running`, the task is "active" and `Execute` is re-driven by the owner when an async signal arrives (see §6 — the wrapped ability task delegate calls `OwnerAbility->AdvanceGraph()`). When status != `Running`, `OnExit` runs and the parent (Sequence) advances. `UAuraSequenceTask::Execute` runs `ChildTasks[ActiveChildIndex]->Execute`; on `Success` advances, on `Failure` aborts, on `Running` waits. This mirrors `UBehaviorUCompositeTask` (`BehaviorUBehaviorTask.cpp:174-199`) and `UBehaviorUSelectorTask`/`SequenceTask` (`BehaviorUComposites.cpp`).

### 4.5 `UAuraDataAbility : public UAuraGameplayAbility`  (the driver)
`DataAbility.h`:
```cpp
UCLASS()
class AURAABILITYGRAPH_API UAuraDataAbility : public UAuraGameplayAbility
{
    GENERATED_BODY()
public:
    // Shared cost/cooldown GEs (SetByCaller-driven; set on the CDO, never per-ability)
    UPROPERTY(EditDefaultsOnly, Category="Cost") TSubclassOf<UGameplayEffect> SharedCostGE;       // GE_AbilityCost_SBC
    UPROPERTY(EditDefaultsOnly, Category="Cooldown") TSubclassOf<UGameplayEffect> SharedCooldownGE; // GE_AbilityCooldown_SBC

    virtual void ActivateAbility(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo*, const FGameplayAbilityActivationInfo, const FGameplayEventData*) override;
    virtual void EndAbility(...) override;
    virtual bool CheckCost(...) const override;        // use Definition->ManaCost
    virtual void ApplyCooldown(...) const override;    // use Definition->CooldownTag + Duration via SBC
    // Look up the Definition for this spec (CurrentSpec.SourceObject, or DA_AbilityInfo by AbilityTag)
    const UAuraAbilityDefinition* GetDefinition() const;

    // Called by async action tasks when their delegate fires (advances the graph)
    void AdvanceGraph(EAuraAbilityActionStatus ChildStatus);
};
```
**`ActivateAbility`:** `CommitAbility()` (cost+cooldown via the SBC GEs + the overrides), `GetDefinition()` → build the root `UAuraAbilityActionTask` (`NewObject<UAuraSequenceTask>(this)` → `Init(RootNode, this)`), store it, run `RootTask->Execute(Ctx)`. If it returns `Running`, stay active; an async node's wrapped ability-task delegate calls `AdvanceGraph(...)` which re-drives `RootTask->Execute`. When root returns `Success`/`Failure`, `EndAbility`. `EndAbility`/`Cancel` walks the task tree calling `Cancel(Ctx)` to tear down any pending ability tasks (montage, wait-event, target-data).

**`GetDefinition()` strategy — pick one (see §7 for the grant mechanism):**
- **Option A (MVP, simple):** `CurrentSpec->SourceObject` is the `UAuraAbilityDefinition*` (set at grant time). `GetDefinition` = `Cast<UAuraAbilityDefinition>(CurrentSpec->GetSourceObject())`.
- **Option B (no-BP):** look up by `GetAbilityTagFromSpec` → `DA_AbilityInfo` row → a new `Definition` field on `FAuraAbilityInfo`.

The plan uses **Option A** for the MVP grant path (§7).

---

## 5. Built-in action node catalog

Each node wraps an existing GAS primitive. All `BlueprintCallable` workhorses below already exist — nodes call them, passing the avatar/ASC from the execution context.

| Node class (`<node class="…">`) | Wraps | Key properties | Behavior |
|---|---|---|---|
| `Sequence` | (composite) | children | Runs children in order; `Success` advances, `Failure` aborts. Mirror `UBehaviorUSequenceTask`. |
| `WaitForTargetData` | `UTargetDataUnderMouse` (`TargetDataUnderMouse.cpp:11`) | (none) | Spawns the task; on `ValidData` caches the `FHitResult` into `Ctx.CursorHit` and returns `Success`. Async (returns `Running` until data arrives). |
| `PlayMontage` | `UAbilityTask_PlayMontageAndWait` | `montageProperty` (resolve from Definition) | Plays `Definition->Montage`; returns `Success` immediately (montage plays in parallel) OR `Running` until montage completes (configurable). |
| `WaitForMontageEvent` | `UAbilityTask_WaitGameplayEvent` | `tag` | Returns `Running` until the `Event.Montage.*` event fires (from `AN_MontageEvent`), then `Success`. |
| `SpawnProjectile` | `UAuraProjectileSpell::SpawnProjectile` (`AuraProjectileSpell.cpp:21`) | `socketTag`, `projectileClass`, `targetFromContext` | Server-only; spawn one `AAuraProjectile` subclass from the socket toward `Ctx.CursorHit.ImpactPoint`; wire `DamageEffectParams` from the Definition's damage fields (build via a helper equivalent to `MakeDamageEffectParamsFromClassDefaults`). Instant `Success`. |
| `SpawnProjectiles` | `UAuraFireBolt::SpawnProjectiles` (`AuraFireBolt.cpp:105`) | `socketTag`, `projectileClass`, `count`, `spread`, `homing`, `homingAccelMin/Max`, `targetFromContext` | Multi-projectile + homing. Instant `Success`. |
| `ApplyDamage` | `UAuraAbilitySystemLibrary::ApplyDamageEffect` (`AuraAbilitySystemLibrary.cpp:823`) | `targetFromContext` (`CursorHit.HitActor`) | Build `FDamageEffectParams` from Definition + target, apply. Instant `Success`. |
| `CauseDamage` | `UAuraDamageGameplayAbility::CauseDamage` (`AuraDamageGameplayAbility.cpp:10`) | `targetFromContext` | Direct GE to target ASC (beam/melee path). Instant. |
| `MulticastGunFX` | `AAuraCharacterBase::MulticastPlayGunFireFX` (`AuraCharacterBase.cpp`) | `muzzleSocketTag`, `muzzleEffect`, `fireSound` | Call the avatar's muzzle-FX NetMulticast. Instant. |
| `HitscanTrace` | `UKismetSystemLibrary::LineTraceSingle` + `ApplyDamageEffect` | `socketTag`, `traceRange`, `scatterRadius`, `targetFromContext` | (For any future hitscan ability — kept from the earlier FireGun hitscan design.) Instant. |
| `TraceFirstTarget` / `StoreAdditionalTargets` | `UAuraBeamSpell` methods (`AuraBeamSpell.cpp:32`,`:72`) | `range`, `maxTargets` | For Electrocute. Bind death delegates (the ability `EndAbility` cleans up). |
| `SpawnFireBalls` | `UAuraFireBlast::SpawnFireBalls` (`AuraFireBlast.cpp:76`) | `count`, `fireBallClass` | For FireBlast. |

**Shared context (`FAuraAbilityExecutionContext`)** — passed by reference into every `Execute`/`OnEnter`/etc.:
```cpp
struct FAuraAbilityExecutionContext
{
    UAbilitySystemComponent* ASC = nullptr;
    AActor* AvatarActor = nullptr;
    FGameplayAbilitySpecHandle SpecHandle;
    FGameplayAbilityActorInfo ActorInfo;       // or pointer
    FHitResult CursorHit;                      // cached by WaitForTargetData
    const UAuraAbilityDefinition* Definition = nullptr;
};
```
This replaces BehaviorU's "agent blackboard" with an ability-scoped context. Nodes read `Ctx.CursorHit`, `Ctx.AvatarActor`, etc.

---

## 6. Async execution & the "tick" mechanism

GAS abilities don't tick. Async nodes wrap **GAS ability tasks** (`UAbilityTask` subclasses), which are latent and tick themselves. Flow:

1. `UAuraDataAbility::ActivateAbility` runs `RootTask->Execute(Ctx)`.
2. `Sequence` runs child 0 (`WaitForTargetData`), which spawns `UTargetDataUnderMouse`, binds its `ValidData` delegate to `OwnerAbility->OnTargetDataReady`, returns `Running`.
3. The ability is now active, waiting. When `UTargetDataUnderMouse::ValidData` fires (client sent cursor data, server received), `OnTargetDataReady` caches `Ctx.CursorHit` and calls `AdvanceGraph(Success)`.
4. `AdvanceGraph` re-drives `RootTask->Execute(Ctx)`. `Sequence` sees child 0 completed `Success`, advances to child 1 (`PlayMontage`), runs it, … and so on through `WaitForMontageEvent` (async, waits for the `AN_MontageEvent` → `UAbilityTask_WaitGameplayEvent` delegate) → `SpawnProjectile` (instant) → `MulticastGunFX` (instant) → root `Success` → `EndAbility`.

`AdvanceGraph` is the single re-entry point. Each async node, in `OnEnter`/`OnStart`, binds its wrapped ability-task delegate to a lambda that calls `OwnerAbility->AdvanceGraph(status)`. `Cancel` (on `EndAbility`) unbinds and `EndTask()`s any pending ability tasks. This mirrors how `UBehaviorUActionTask` uses `EnqueueMethodCommand` + polls (`BehaviorUActions.cpp:43-104`) — but simpler because GAS ability tasks already callback via delegates on the game thread (no worker-thread command queue needed).

**All execution is game-thread.** No worker threads (unlike BehaviorU's Phase-2). GAS ability tasks fire delegates on the game thread. This avoids BehaviorU's two-phase tick complexity entirely.

---

## 7. Grant mechanism & integration with RoleConfig / DA_AbilityInfo / save-load

This is the most invasive part. Today `RoleConfig.json` `lmbAbility` / `startupAbilities` are **BP class paths** (`TSubclassOf<UGameplayAbility>`), granted by `AddCharacterAbilities` (`AuraAbilitySystemComponent.cpp:78`). The rewrite changes them to **ability definition asset paths** and grants the single `UAuraDataAbility` class with the definition attached per-spec.

### 7.1 `RoleConfig.json` changes
`lmbAbility` and each `startupAbilities` entry become **`UAuraAbilityDefinition` asset paths** (e.g. `/Game/AbilityDefs/FireGun.FireGun`), not BP class paths. `FRoleDefaultInfo` (`RoleInfo.h:88`,`:91`,`:99`) changes:
- `TArray<TSubclassOf<UGameplayAbility>> StartupAbilities` → `TArray<TObjectPtr<UAuraAbilityDefinition>> StartupAbilities` (or `TArray<FSoftObjectPath>` resolved at load).
- `TSubclassOf<UGameplayAbility> DefaultLMBAbility` → `TObjectPtr<UAuraAbilityDefinition> DefaultLMBAbility`.
The loader (`AuraAbilitySystemLibrary.cpp` `LoadRoleConfig` ~`:420`) changes `LoadAbilityClasses` to load `UAuraAbilityDefinition` assets instead of `LoadClass<UGameplayAbility>`.

### 7.2 Grant path (`AddCharacterAbilities` rewrite)
New `UAuraAbilitySystemComponent::AddDataAbilities(const TArray<UAuraAbilityDefinition*>&)` (or extend the existing `AddCharacterAbilities` to branch on whether the class is `UAuraDataAbility`):
```cpp
for (UAuraAbilityDefinition* Def : Definitions)
{
    FGameplayAbilitySpec Spec(UAuraDataAbility::StaticClass(), 1);
    Spec.SourceObject = Def;                                   // Option A: per-spec definition
    Spec.DynamicAbilityTags.AddTag(Def->InputTag);             // InputTag.LMB etc.
    Spec.DynamicAbilityTags.AddTag(Def->AbilityTag);           // Abilities.Gun.Fire
    Spec.DynamicAbilityTags.AddTag(Abilities_Status_Equipped);
    GiveAbility(Spec);
}
```
`UAuraDataAbility::GetDefinition()` = `Cast<UAuraAbilityDefinition>(CurrentSpec->GetSourceObject())`.

### 7.3 `GetAbilityTagFromSpec` change (`AuraAbilitySystemComponent.cpp:271`)
Today it reads `Spec.Ability->AbilityTags` (the CDO). With one shared `UAuraDataAbility` class, the CDO has no per-ability tag. Change it to **also** scan `Spec.DynamicAbilityTags` for the first `Abilities.*` tag:
```cpp
FGameplayTag UAuraAbilitySystemComponent::GetAbilityTagFromSpec(const FGameplayAbilitySpec& Spec)
{
    if (Spec.Ability)
        for (FGameplayTag Tag : Spec.Ability->AbilityTags)
            if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag("Abilities"))) return Tag;
    for (FGameplayTag Tag : Spec.DynamicAbilityTags)
        if (Tag.MatchesTag(FGameplayTag::RequestGameplayTag("Abilities"))) return Tag;
    return FGameplayTag();
}
```
This keeps backward compatibility (legacy non-data abilities still work via CDO `AbilityTags`) while data abilities resolve via `DynamicAbilityTags`.

### 7.4 `DA_AbilityInfo` (`AbilityInfo.h`) — **CORRECTED: Now JSON-based**

**Original Plan (INCORRECT):** Add a `Definition` field to `FAuraAbilityInfo` struct in the `DA_AbilityInfo` UAsset.

**Corrected Implementation:** `DA_AbilityInfo` UAsset has been **replaced entirely** with `Content/Config/AbilityInfo.json`. The new `URuntimeAbilityInfo` class loads JSON at runtime and creates transient objects (no UAsset). This matches the RoleConfig.json pattern already used in the project.

`AbilityInfo.json` schema:
```json
{
  "abilities": [
    {
      "abilityTag": "Abilities.Fire.FireBolt",
      "icon": "/Game/UI/GlobeSpecs/FireBolt.FireBolt",
      "backgroundMaterial": "/Game/UI/GlobeSpecs/M_FireBolt.M_FireBolt",
      "levelRequirement": 1
    }
  ]
}
```

`FAuraAbilityInfo` struct now separates:
- **JSON-loaded UI fields:** `Icon`, `BackgroundMaterial`, `LevelRequirement`
- **Runtime-resolved fields:** `InputTag`, `StatusTag`, `CooldownTag`, `AbilityType` (queried from ASC/Definition at lookup time)

**Migration:** Export current `DA_AbilityInfo` entries to JSON, update all `GetAbilityInfo()` calls to `GetRuntimeAbilityInfo()`, delete `DA_AbilityInfo` UAsset.

### 7.5 Save / load (`FSavedAbility`, `SaveProgress_Implementation`, `AddCharacterAbilitiesFromSaveData`)
`FSavedAbility.GameplayAbility` (`LoadScreenSaveGame.h:52`) is a `TSubclassOf<UGameplayAbility>`. For data abilities the class is always `UAuraDataAbility` — saving it is useless. **Save the `AbilityTag`** (already saved) + on load, resolve the `UAuraAbilityDefinition` from `DA_AbilityInfo` by tag (`Info.Definition`), then re-grant with `Spec.SourceObject = Definition` + the saved slot/status tags. Change `AddCharacterAbilitiesFromSaveData` (`AuraAbilitySystemComponent.cpp:47`) to branch: if `Info.Definition` is set → grant `UAuraDataAbility` with `SourceObject=Definition`; else → legacy path. `SaveProgress_Implementation` (`AuraCharacter.cpp:400`) mostly unchanged (it already saves `AbilityTag`/slot/status); just stop relying on `Info.Ability` for data abilities.

### 7.6 `ApplyRole` LMB strip (`AuraCharacterBase.cpp:178-190`)
Today it strips abilities whose CDO `StartupInputTag == InputTag.LMB` and re-adds `DefaultLMBAbility`. With definitions, "strip LMB" = remove any definition whose `InputTag == InputTag.LMB` from the role's `StartupAbilities` list, then add `DefaultLMBAbility` (a definition). Same logic, operating on `UAuraAbilityDefinition*` instead of `TSubclassOf`.

---

## 8. Cost / cooldown without per-ability GE BPs

Today each ability has a hand-built `GE_Cost_<Ability>` and `GE_Cooldown_<Ability>` BP. Replace with **two shared SetByCaller GEs**:

- `GE_AbilityCost_SBC` — a GE with one modifier: `Attributes.Vital.Mana` by `SetByCaller` tag `Abilities.Cost.Mana` (or `Mana` — pick one). Magnitude set at apply time.
- `GE_AbilityCooldown_SBC` — a GE with `Has Duration`, duration by `SetByCaller` tag `Abilities.Cooldown.Duration`, and grants a gameplay tag by `SetByCaller`? **Tags can't be SetByCaller.** So the cooldown *tag* must be granted differently. Two options:
  - (i) Make `GE_AbilityCooldown_SBC` grant a generic `Cooldown.Active` tag, and override `GetCooldownTags()` on `UAuraDataAbility` to return `{Definition->CooldownTag}` so `CheckCooldown`/the ASC cooldown check respects the definition's tag. The applied GE grants `Cooldown.Active` (so the ASC sees *a* cooldown), and `GetCooldownTags` reports the real per-ability tag for matching. This is the standard GAS pattern for data-driven cooldowns.
  - (ii) Spawn a tiny dynamic GE per activation with the right tag — more complex.

  Use **(i)**. `UAuraDataAbility::GetCooldownTags()` returns `FGameplayTagContainer(Definition->CooldownTag)`. `ApplyCooldown` applies `SharedCooldownGE` with SetByCaller duration = `Definition->CooldownDuration`. `CheckCooldown` uses `GetCooldownTags()` so the right tag is checked.

- `UAuraDataAbility::ApplyCooldown(const FGameplayAbilitySpecHandle, const FGameplayAbilityActorInfo*, const FGameplayAbilityActivationInfo) const` override: build a spec from `SharedCooldownGE`, `AssignTagSetByCallerMagnitude(Spec, "Abilities.Cooldown.Duration", Definition->CooldownDuration.GetValueAtLevel(Level))`, apply to self. Same for `ApplyCost` with `Abilities.Cost.Mana` = `Definition->ManaCost`. `CheckCost` checks mana >= `Definition->ManaCost` (skip if 0). `CheckCooldown` checks the definition's cooldown tag.

Add two native tags: `Abilities.Cost.Mana`, `Abilities.Cooldown.Duration` (in `AuraGameplayTags.cpp`). The two shared GEs are created once as assets (one-time editor op; not per-ability).

This **eliminates every `GE_Cost_*` and `GE_Cooldown_*` BP**. The cooldown *duration* and *tag* and the mana *cost* all live in the XML.

---

## 9. Migration plan (phased)

### Phase 1 — Framework (no behavior change yet)
1. Create `AuraAbilityGraph` + `AuraAbilityGraphEditor` modules; add to `Aura.uproject`; build.
2. Implement `FAuraAbilityNodeRegistry`, `UAuraAbilityActionNode`, `UAuraAbilityActionTask`, `UAuraAbilityDefinition` (with `LoadFromXML` copied from `BehaviorUBehaviorTree.cpp`), `UAuraSequenceTask`.
3. Implement `UAuraDataAbility` driver (`ActivateAbility`/`EndAbility`/`AdvanceGraph`/`GetDefinition`/cost+cooldown overrides). Set `SharedCostGE`/`SharedCooldownGE` CDO pointers (create the two SBC GE assets in the editor).
4. Implement the import factory (`UAbilityDefinitionImportFactory`, mirror `BehaviorUBehaviorTreeFactory.cpp`) so XML imports into a `UAuraAbilityDefinition` data asset, with reimport.
5. Register built-in nodes in `FAuraAbilityGraphModule::StartupModule`: `Sequence`, `WaitForTargetData`, `PlayMontage`, `WaitForMontageEvent`, `SpawnProjectile`, `SpawnProjectiles`, `ApplyDamage`, `CauseDamage`, `MulticastGunFX`, `HitscanTrace`.
6. Build, verify no compile errors (run `check-build-errors` against `C:\Git\UnrealEngine-5.5\Engine\Programs\UnrealBuildTool\Log.txt`).

**Deliverable:** the framework compiles and imports an XML file into a data asset. No abilities ported yet; legacy abilities still run.

### Phase 2 — MVP port: `FireGun` (the simplest end-to-end ability)
1. Author `Content/AbilityDefs/FireGun.xml` (§3 example).
2. Import → `UAuraAbilityDefinition` asset. Verify the import (open the asset; check fields populated).
3. Implement the grant path for data abilities: `RoleConfig.json` BungeeMan `lmbAbility` → the `FireGun` definition asset path; update `LoadRoleConfig` + `FRoleDefaultInfo` + `ApplyRole` LMB strip + `AddCharacterAbilities` (§7.1–7.3, §7.6). Change `GetAbilityTagFromSpec` (§7.3).
4. Delete (or disable) `GA_FireGun` BP and `UAuraFireGun` C++ — the XML + `UAuraDataAbility` replaces them. Keep `AAuraBullet`, `BP_AuraBullet`, `MulticastPlayGunFireFX` (the `MulticastGunFX` node calls the existing multicast).
5. PIE test as BungeeMan: press LMB → `[FireGun]`-equivalent log (add a log in `SpawnProjectileNode`/`UAuraDataAbility::ActivateAbility`), bullet spawns from muzzle toward cursor, impact FX on hit, cooldown gates fire rate, FX replicate to other clients.
6. Verify save/load: save with the gun equipped, reload, confirm it re-grants from `DA_AbilityInfo` (add the `Definition` field + a FireGun row).

**Deliverable:** one fully data-driven ability working end-to-end, replacing its BP + C++ class. The grant/save/RoleConfig path is proven.

### Phase 3 — Port the remaining player abilities
Port in this order (simplest → most complex):
1. `FireBolt` (multi-projectile + homing) — `SpawnProjectiles` node.
2. `ArcaneShards` (radial damage at cursor) — `ApplyDamage`/`CauseDamage` node + radial fields in `<damage>`.
3. `FireBlast` (spawn fireballs, return-explode) — `SpawnFireBalls` node + an OnReturn handler node.
4. `Electrocute` (beam chain) — `TraceFirstTarget` + `StoreAdditionalTargets` nodes + a loop; the `PrimaryTargetDied`/`AdditionalTargetDied` BPI events become `OnTargetDied` nodes that spawn death FX.
5. Passives (`HaloOfProtection`, `LifeSiphon`, `ManaSiphon`) — a `ListenForEvent` + `ApplyEffect` node pair (mirror the `GA_ListenForEvent` + `GE_EventBasedEffect` template).

For each: author XML, import, add a `DA_AbilityInfo` row (with `Definition`), update `RoleConfig`/`startupAbilities` to the definition path, delete the old BP + C++ class. Keep the old C++ workhorses' *logic* inside node `.cpp`s (copy `UAuraFireBolt::SpawnProjectiles` body into `USpawnProjectilesNode`'s task, etc.).

### Phase 4 — Enemy abilities + cleanup
1. Port enemy abilities (`GA_EnemyFireBolt`, `GA_MeleeAttack`, `GA_RangedAttack`, `GA_SummonAbility`, `GA_HitReact`) — these use the same nodes; enemies grant data abilities via their own startup path.
2. Delete all `GE_Cost_*` / `GE_Cooldown_*` BPs (replaced by the two shared SBC GEs).
3. Delete all `GA_*` BP event graphs (replaced by definitions). Keep projectile BPs (`BP_FireBolt`, `BP_AuraBullet`, `BP_FireBall`) for now (mesh/FX defaults) — Phase 5 candidate.
4. Update `AuraGameplayTags.cpp` — remove now-unused per-ability cooldown tags? **No** — keep `Cooldown.Fire.FireBolt` etc.; they're still referenced by the definitions' `<cooldown tag>`. They just no longer need native registration if authored as plain tags, but leaving them native is harmless.

### Phase 5 (optional, future) — Projectile data-driven
Move projectile mesh/FX/impact params into a `UAuraProjectileDefinition` data asset so `BP_FireBolt`/`BP_AuraBullet`/`BP_FireBall` also disappear. Out of scope for this plan.

---

## 10. Decisions to make before coding (resolve with the user if unclear)

**CORRECTED DECISIONS (Already Applied):**

1. **Asset references in XML: hard or soft?** ✅ **RESOLVED:** Hard refs via `LoadObject` at XML parse time (matches BehaviorU). Runtime XML loading means no cooking concerns—assets load on-demand when the XML is read.

2. **Per-ability BP elimination:** ✅ **RESOLVED:** Option A confirmed—`Spec.SourceObject = Definition` (transient, loaded from XML). **Zero UAssets** for abilities. All behavior driven by XML files on disk.

3. **Montage asset:** ✅ **RESOLVED:** Montages remain as UAssets (authored once, referenced by path in XML). This is unavoidable animation authoring, not ability-logic authoring. Acceptable.

4. **Projectile BPs:** ✅ **RESOLVED:** Kept in Phase 1-4 (mesh/FX defaults). Phase 5 (optional future work) could make them data-driven too.

5. **Backward compatibility:** ✅ **RESOLVED:** Legacy `GetAbilityInfo()` (UAsset-based) and `UAbilityInfo` class kept with deprecation warnings. New code uses `GetRuntimeAbilityInfo()` and `URuntimeAbilityInfo` (JSON-based). Mixed operation during migration is supported.

6. **Import factory role:** ✅ **CLARIFIED:** The import factory (`UAbilityDefinitionImportFactory`) exists **only for editor convenience** (drag-and-drop XML into Content Browser for preview). The runtime system **does not use it**—it reads XML directly from disk with `LoadAbilityDefinitionFromXMLFile()`. This matches BehaviorU's design exactly.

7. **DA_AbilityInfo replacement:** ✅ **RESOLVED:** Replaced with `Content/Config/AbilityInfo.json` loaded at runtime into `URuntimeAbilityInfo` (transient). No UAsset dependency. Matches RoleConfig.json pattern.

---

## 11. Verification (per phase)

- **Build:** after each phase, close the editor (Live Coding blocks full links — see the `check-build-errors` skill) and rebuild `AuraEditor`. Grep `C:\Git\UnrealEngine-5.5\Engine\Programs\UnrealBuildTool\Log.txt` for `error C|error LNK|fatal error|unresolved external`. Ignore stale VS Code IntelliSense (`pp_file_not_found`) — the build log is the source of truth.
- **Import:** import each XML; open the resulting `UAuraAbilityDefinition` asset and confirm all fields populated (tags, cost, cooldown, damage, montage, node tree).
- **PIE single player:** `defaultRole: BungeeMan`; press LMB; confirm `[DataAbility]`/node log lines, the bullet spawns, damage applies (enemy dies), cooldown gates, muzzle FX play. Check `Saved/Logs/Aura.log` for the execution trace.
- **PIE 2 clients + listen server:** confirm FX replicate (NetMulticast) and damage is server-authoritative (no double-damage). Confirm save → reload restores the ability.
- **Regression:** Aura role's `FireBolt` still works after Phase 3 (run Aura role, press LMB, confirm FireBolt fires).

---

## 12. Risks & gotchas

- **`FXmlFile` single-line bug:** the prolog-strip + newline-after-`>` preprocessing in `BehaviorUBehaviorTree.cpp:200-217` is mandatory. Copy it verbatim or one-line XML imports blank.
- **`GetAbilityTagFromSpec` change (§7.3) touches a hot path** (UI, save, equip, cooldown, passives). Test the UI spell menu, equip/unequip, and passive activation after the change. Keep the CDO-`AbilityTags` scan first so legacy abilities are unaffected.
- **Cooldown via shared GE + `GetCooldownTags` override (§8):** verify the ASC's cooldown check actually consults `GetCooldownTags()` (it does in UE5.3+). If a per-ability cooldown tag must be *granted* (not just reported), fall back to spawning a dynamic tagged GE per activation (option ii).
- **Async node cleanup:** `UAuraDataAbility::EndAbility` MUST walk the task tree and `Cancel` every pending ability task (`PlayMontageAndWait`, `WaitGameplayEvent`, `TargetDataUnderMouse`) or they leak / fire after end. Mirror `UBehaviorUBehaviorTask::Reset`.
- **Game-thread only:** all node execution and ability-task delegates run on the game thread. Do **not** port BehaviorU's worker-thread/two-phase tick — it's unnecessary here and would break GAS game-thread requirements.
- **Cooking:** `UAuraAbilityDefinition` is a `UDataAsset` referenced by `DA_AbilityInfo` / `RoleConfig`-loaded soft refs — it cooks normally. The XML file itself is not needed at runtime (the imported data asset is the cooked artifact), unlike BehaviorU which reads XML from disk at runtime. This is **better** for cooking. The import factory + reimport handle the XML→asset step in-editor.
- **`SourceObject` on `FGameplayAbilitySpec`:** confirm `FGameplayAbilitySpec::SourceObject` (a `TWeakObjectPtr<UObject>`) survives serialization/replication. It does not replicate — but the spec is rebuilt on the server and re-granted on load from `DA_AbilityInfo`, so `SourceObject` is set on the granting side (server). Clients receive the spec via standard GAS spec replication; verify the client's `CurrentSpec->SourceObject` is valid (if not, fall back to Option B: DA_AbilityInfo lookup by the replicated `AbilityTag` in `DynamicAbilityTags`).

---

## 13. Concrete file-level checklist for the implementing agent

**New files to create:**
- `Source/AuraAbilityGraph/AuraAbilityGraph.Build.cs`, `Public/AuraAbilityGraphModule.h`, `Private/AuraAbilityGraphModule.cpp`
- `Source/AuraAbilityGraph/Public/AbilityGraphTypes.h`, `AbilityDefinition.h`, `AbilityNodeRegistry.h`, `DataAbility.h`
- `Source/AuraAbilityGraph/Private/AbilityDefinition.cpp`, `AbilityNodeRegistry.cpp`, `DataAbility.cpp`
- `Source/AuraAbilityGraph/Public/Nodes/AbilityActionNode.h`, `Nodes/AbilityActionTask.h`, `Nodes/Composites/SequenceNode.h`
- `Source/AuraAbilityGraph/Private/Nodes/AbilityActionNode.cpp`, `Nodes/AbilityActionTask.cpp`, `Nodes/Composites/SequenceNode.cpp`
- `Source/AuraAbilityGraph/Public/Nodes/Actions/{WaitForTargetDataNode,PlayMontageNode,WaitForMontageEventNode,SpawnProjectileNode,SpawnProjectilesNode,ApplyDamageNode,CauseDamageNode,MulticastGunFXNode,HitscanTraceNode}.h` + matching `Private/.../*.cpp`
- `Source/AuraAbilityGraphEditor/AuraAbilityGraphEditor.Build.cs`, `Public/AuraAbilityGraphEditorModule.h`, `Private/AuraAbilityGraphEditorModule.cpp`, `Private/AbilityDefinitionImportFactory.cpp` (+ `.h`)
- `Aura.uproject` — add the two modules.

**Existing files to edit:**
- `Source/Aura/Public/AbilitySystem/Data/RoleInfo.h` — `FRoleDefaultInfo`: `StartupAbilities`/`StartupPassiveAbilities`/`DefaultLMBAbility` → `UAuraAbilityDefinition*` (or `TSoftObjectPtr`).
- `Source/Aura/Private/AbilitySystem/AuraAbilitySystemLibrary.cpp` — `LoadRoleConfig`/`LoadAbilityClasses`: load definition assets instead of ability classes.
- `Source/Aura/Private/Character/AuraCharacterBase.cpp:155-198` — `ApplyRole` LMB strip on definitions.
- `Source/Aura/Private/AbilitySystem/AuraAbilitySystemComponent.cpp:47`,`:78`,`:271` — `AddCharacterAbilities`/`AddCharacterAbilitiesFromSaveData`/`GetAbilityTagFromSpec` (§7.2, §7.3, §7.5).
- `Source/Aura/Public/AbilitySystem/Data/AbilityInfo.h` — `FAuraAbilityInfo`: add `Definition` field.
- `Source/Aura/Public/Game/LoadScreenSaveGame.h:52` — `FSavedAbility`: keep `AbilityTag` as the key; `GameplayAbility` becomes optional/legacy.
- `Source/Aura/Private/AuraGameplayTags.cpp` — add `Abilities.Cost.Mana`, `Abilities.Cooldown.Duration`; keep all existing `Cooldown.*` tags.
- `Content/Config/RoleConfig.json` — `lmbAbility`/`startupAbilities` → definition asset paths (per role).
- `Content/AbilityDefs/FireGun.xml` (and per-ability XMLs in Phase 3).

**Editor asset ops (one-time, the user does in-editor):**
- Create `GE_AbilityCost_SBC` (Mana modifier, SetByCaller `Abilities.Cost.Mana`) and `GE_AbilityCooldown_SBC` (Has Duration, SetByCaller `Abilities.Cooldown.Duration`, grants `Cooldown.Active`). Set both as `SharedCostGE`/`SharedCooldownGE` on the `UAuraDataAbility` CDO (or a single `BP_DataAbility` that sets them + is the granted class — one BP, one time, no graph).
- Import each XML into a `UAuraAbilityDefinition` asset (via the new import factory).
- Add a `DA_AbilityInfo` row per data ability (`AbilityTag`, `CooldownTag`, `AbilityType`, `Icon`, `LevelRequirement`, `Definition`).
- Add native tags `Abilities.Cost.Mana`, `Abilities.Cooldown.Duration`, `Cooldown.Active` to `Config/DefaultGameplayTags.ini` if not native-registered.

---

## 14. One-paragraph summary for the agent

Build a new `AuraAbilityGraph` module that mirrors BehaviorU's XML→UDataAsset→registry→node-tree pattern: `UAuraAbilityDefinition` (UDataAsset) is imported from an ability XML file and holds the ability's tags, cost, cooldown, damage, montage, and an action-node tree; `FAuraAbilityNodeRegistry` lets modules register C++ action nodes; `UAuraAbilityActionNode`/`UAuraAbilityActionTask` are the definition/runtime base classes (copy `UBehaviorUBehaviorNode`/`UBehaviorUBehaviorTask`); a single `UAuraDataAbility : UAuraGameplayAbility` drives the node tree on activation, advancing through nodes that wrap existing GAS primitives (`UTargetDataUnderMouse`, `PlayMontageAndWait`, `WaitGameplayEvent`, `SpawnProjectile`, `ApplyDamageEffect`, `MulticastPlayGunFireFX`) — reusing all current C++ workhorses, no behavior rewrite. Cost/cooldown use two shared SetByCaller GEs (no per-ability GE BPs). Grant `UAuraDataAbility` with `FGameplayAbilitySpec::SourceObject = Definition` + per-spec `DynamicAbilityTags` for the ability/input tags; update `GetAbilityTagFromSpec` to read `DynamicAbilityTags`, `RoleConfig.json` to use definition asset paths, `ApplyRole`/`AddCharacterAbilities`/save-load accordingly, and `DA_AbilityInfo` to carry a `Definition` pointer. Migrate FireGun first as MVP, then the rest, deleting each ability's BP event graph + cost/cooldown GEs + per-ability C++ class as it's ported. Verify each phase with a closed-editor rebuild (`check-build-errors`) and PIE (single + 2-client).