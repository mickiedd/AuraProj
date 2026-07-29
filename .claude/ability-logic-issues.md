# Ability Logic Issues — AuraProj

Analysis date: 2026-07-29 (initial), 2026-07-29 (updated with GE audit findings)
Scope: `Plugins/AuraAbilityGraph`, `Source/Aura/AbilitySystem`, `Plugins/AuraAbilityGraph/Editor`

## Architecture Overview

The ability system is a data-driven, XML-parsed, node-graph architecture:

1. **`UAuraAbilityDefinition`** (`AbilityDefinition.h/cpp`) — Parses XML ability definitions from disk. Contains identity, cost/cooldown, damage, animation, and a graph of action nodes.
2. **`UAuraDataAbility`** (`DataAbility.h/cpp`) — The `UGameplayAbility` subclass that executes the ability graph. Manages lifecycle: `ActivateAbility` → cost/cooldown → build graph → `RootTask->Execute()` → `AdvanceGraph()` on async completions → `EndAbility`.
3. **`UAuraAbilityActionNode`** / **`UAuraAbilityActionTask`** — Base node/task classes. Nodes are defined in XML and instantiated via `FAuraAbilityNodeRegistry`. Tasks execute `OnStart()` and return `EAuraAbilityActionStatus` (Success/Failure/Running).
4. **`UAuraSequenceTask`** (`SequenceNode.cpp`) — Composite node that executes children in order. Advances `ActiveChildIndex` on success, fails on first failure, returns `Running` if any child is async.
5. **Action Nodes** — `ApplyDamage`, `CauseDamage`, `HitscanTrace`, `SpawnProjectile`, `SpawnProjectiles`, `PlayMontage`, `WaitForMontageEvent`, `WaitForTargetData`, `FaceTarget`, `MulticastGunFX`.
6. **C++ GameplayEffects** — 6 classes replacing all GE UAssets: `UAuraDamageGameplayEffect`, `UAuraManaCostGameplayEffect`, `UAuraCooldownGameplayEffect`, `UAuraAttributeGameplayEffect`, `UAuraAttributeGameplayEffect_Infinite`, `UAuraPickupGameplayEffect`. Config in `Content/Config/GameplayEffects.json`.
7. **`UAuraAbilitySystemLibrary`** — Core library with `ApplyDamageEffect()`, `LoadAbilityDefinitionFromXMLFile()`, `LoadRoleInfoFromConfig()`, `InitializeDefaultAttributes*()`.
8. **`FDamageEffectParams`** (`AuraAbilityTypes.h`) — Struct holding all damage parameters for `ApplyDamageEffect`.
9. **`FAuraGameplayEffectContext`** (`AuraAbilityTypes.h`) — Custom `FGameplayEffectContext` subclass with additional fields (crit, block, debuff, radial, etc.).
10. **Web Editor** — `Plugins/AuraAbilityGraph/Editor/` (Python HTTP server + JS frontend, 4 tabs: Ability Graph, Ability Info, Role Config, GE Config).

---

## Issues

### CRITICAL — Infrastructure / Security

#### C1. Binary and cache files committed to git — UNFIXED
- 162 `WebView2UserData/` browser cache files (history, cookies, leveldb, GPUCache, Code Cache, etc.) tracked in git under `Plugins/AuraAbilityGraph/AuraAbilityGraphLauncher/`
- `AuraAbilityGraphLauncher.exe`, `.pdb`, `WebView2Loader.dll`, `.vcxproj` also committed
- `.gitignore` covers `/Binaries/` and `/Intermediate/` but not launcher build artifacts or `WebView2UserData/`
- **Fix**: Add `.gitignore` entries for `*.exe`, `*.pdb`, `*.dll`, `WebView2UserData/`, `*.vcxproj` under launcher folder; `git rm --cached` the tracked files

#### C2. Path traversal in `ability_graph_server.py` — UNFIXED
- `/load` endpoint reads arbitrary file paths with zero validation (`ability_graph_server.py:116-135`)
- `/save` endpoint accepts arbitrary `source_path` — only checks `.xml` extension but allows absolute paths anywhere on disk
- `/save-ability-info` and `/save-role-config` resolve paths from `os.getcwd()` which could be manipulated
- Server binds to `0.0.0.0` (line 302) — accessible from the network, not just localhost
- No CORS headers — any web page on the machine can POST to these endpoints
- **Fix**: Restrict `/load` and `/save` to project Content directory; reject `..` traversal; bind to `127.0.0.1`

#### C3. `SourceObject` does not replicate — UNFIXED
- `FGameplayAbilitySpec::SourceObject` is a `TWeakObjectPtr<UObject>` that doesn't replicate
- Server grants the ability with `SourceObject = Definition`, but clients receive the spec via standard GAS spec replication
- If `SourceObject` is null on the client, `GetDefinition()` returns nullptr, and `ActivateAbility` aborts at line 103-108
- **Impact**: Data-driven abilities may silently fail on clients in multiplayer
- **Fix**: Add a fallback in `GetDefinition()` — look up the definition by `AbilityTag` from `DynamicAbilityTags` via a runtime definition registry or `DA_AbilityInfo`

---

### HIGH — Logic Bugs

#### H1. PlayMontage delegates not wired — UNFIXED
- `PlayMontageNode.cpp:33-42` creates `UAbilityTask_PlayMontageAndWait` but does **not bind** its `OnComplete` / `OnInterrupted` / `OnBlendOut` delegates
- `UAuraDataAbility::OnMontageCompleted()` and `OnMontageInterrupted()` UFUNCTIONs exist but are never called from C++
- **Impact**: If the montage is interrupted before the gameplay event fires, the graph hangs in `Running` state forever — `EndAbility` is never triggered
- **Fix**: Bind `OnInterrupted` delegate in `PlayMontageTask::OnStart` to call `OwnerAbility->OnMontageInterrupted()`

#### H2. Inconsistent knockback force direction — UNFIXED
- `ApplyDamageNode.cpp:67` uses `Direction * KnockbackForceMagnitude` (toward target)
- `HitscanTraceNode.cpp:96`, `SpawnProjectileNode.cpp:109`, `SpawnProjectilesNode.cpp:135` all use `FVector::UpVector * KnockbackForceMagnitude` (straight up)
- **Impact**: Hitscan and projectile abilities knock targets upward while direct apply damage knocks them away from the source

#### H3. `CheckCost` comment claims client bypass but implementation doesn't do it — UNFIXED
- `AuraGameplayAbility.h:26-28` comment says "Skip the cost check on non-authoritative clients — attributes (e.g. Mana) may not have replicated yet."
- `AuraGameplayAbility.cpp:9-13` just delegates to `Super::CheckCost` with no client-side bypass
- **Impact**: On clients, mana cost check may fail if mana hasn't replicated yet, preventing ability activation

---

### MEDIUM — Dead Code / Unused Features

#### M4. Dead declarations — UNFIXED
- `AbilityDefinition.h:62` declares `static UAuraAbilityActionNode* CreateNodeByClassName(const FString& ClassName, UObject* Outer)` — never defined. A file-scope function with the same name exists in `AbilityDefinition.cpp` but the member function is missing
- `DataAbility.h:66` declares `void BuildAndExecuteGraph(FAuraAbilityExecutionContext& Ctx)` — never defined, never called
- **Fix**: Remove both declarations

#### M5. Parsed-but-unused XML properties — UNFIXED
- `TargetFromContext` on `SpawnProjectileNode`, `SpawnProjectilesNode`, `ApplyDamageNode`, `CauseDamageNode` — parsed from XML but `OnStart` always uses `Ctx.CursorHit.ImpactPoint` / `Ctx.CursorHit.GetActor()` — the property is never consulted
- `HitscanTraceNode::ScatterRadius` — parsed from XML but the trace is a straight `LineTraceSingleByChannel` with no scatter applied
- **Impact**: XML authors get the illusion of configurability that doesn't actually work
- **Fix**: Either implement the properties or remove them from the schema and `LoadFromProperties`

#### M6. Fake smoke tests — UNFIXED
- `SmokeTest_NodeRegistry` (`AuraAbilityGraphModule.cpp:92-117`) iterates a hardcoded string array and logs — never calls `FAuraAbilityNodeRegistry::Get().Create()`. Always returns true
- `SmokeTest_SequenceExecution` (`AuraAbilityGraphModule.cpp:119-123`) logs a message and returns true — no execution is tested
- Expected node list in `SmokeTest_NodeRegistry` is missing `FaceTarget` even though it's registered (line 614)
- **Fix**: Replace with real tests or mark as stubs explicitly

#### M7. `PlayMontageTask::OnStart` returns `Success` when no montage — UNFIXED
- `PlayMontageNode.cpp:29` — returns `EAuraAbilityActionStatus::Success` instead of `Failure` when `Definition->Montage` is null
- **Impact**: Missing montage assets are silently skipped rather than signaling an error

#### M8. `WaitForMontageEventTask` — no validation of EventTag — UNFIXED
- `WaitForMontageEventNode.cpp:38-46` — If neither the node's `EventTag` nor the definition's `MontageEventTag` is set, `EventTag` will be invalid
- `UAbilityTask_WaitGameplayEvent::WaitGameplayEvent` with an invalid tag may never fire
- **Impact**: Ability graph could hang indefinitely waiting for an event that never fires
- Note: current XML definitions always set `eventTag`, so this is a defensive concern

#### M9. Two separate damage application paths with different context setup — UNFIXED
- `ApplyDamageNode` uses `UAuraAbilitySystemLibrary::ApplyDamageEffect(Params)` which internally creates its own GE spec and context
- `CauseDamageNode` uses `DataAbility->MakeOutgoingGameplayEffectSpec` + `SourceASC->ApplyGameplayEffectSpecToTarget` directly
- **Impact**: These two paths could produce different results if the GE context setup differs

#### M10. `WaitForMontageEventTask::OnEventReceived` bypasses `OnMontageEventReceived` — UNFIXED
- `WaitForMontageEventNode.cpp:70-79` — `OnEventReceived` calls `AdvanceGraph` directly, bypassing `DataAbility::OnMontageEventReceived`
- `DataAbility::OnMontageEventReceived` (line 312-327) also calls `AdvanceGraph`
- **Impact**: If both fire for the same event, `AdvanceGraph` is called twice. The second call is a no-op but it's a code smell

#### M11. `WaitForTargetDataTask::OnValidData` no active-graph check — UNFIXED
- `WaitForTargetDataNode.cpp:45-53` — `OnValidData` calls `DataAbility->OnTargetDataReady()` without checking `bGraphActive`
- `PendingStatus` is set to `Success` unconditionally, which could cause issues if the ability has already ended

---

### LOW — Code Quality / Observations

#### L12. GC: `Montage` and `DamageEffectClass` have no UPROPERTY on Transient UObject — UNFIXED
- `AbilityDefinition.h:49` — `TObjectPtr<UAnimMontage> Montage` has no `UPROPERTY`
- `AbilityDefinition.h:39` — `TSubclassOf<UGameplayEffect> DamageEffectClass` has no `UPROPERTY`
- In a `Transient` UObject, non-UPROPERTY TObjectPtrs are not GC-scanned. Loaded assets are typically root-set by the loader so this works in practice, but it's fragile
- `RootNode` correctly has `UPROPERTY(Instanced)`

#### L13. Hardcoded machine-specific Python path in launcher — UNFIXED
- `AuraAbilityGraphLauncher.cpp:193` — `L"C:\\Users\\Administrator\\AppData\\Local\\Programs\\Python\\Python311\\python.exe"`
- Won't work on other developers' machines. The fallback to `python`/`python3`/`py` in PATH is the real mechanism

#### L14. `CooldownDuration` doesn't scale from XML — UNFIXED
- `AbilityDefinition.cpp:138` — `CooldownDuration.Value = FCString::Atof(*DurationStr)` sets only the base value, ignoring the `ScalableFloat` curve
- **Impact**: Cooldowns don't scale with ability level from XML

#### L15. `FDamageEffectParams::WorldContextObject` never populated — UNFIXED
- `AuraAbilityTypes.h:16` — declared but always `nullptr` when passed from ability nodes
- `ApplyDamageEffect` never reads it

#### L16. `AbilityDefinitionImportFactory::CanReimport` always returns false — UNFIXED
- `AbilityDefinitionImportFactory.cpp:47-58` — factory doesn't support reimport, `SourceFilePath` not stored on the asset
- **Impact**: XML changes can't be reimported into an existing asset; must delete and re-import

#### L17. `MulticastGunFXNode` — no fallback if avatar is not `AAuraCharacterBase` — UNFIXED
- `MulticastGunFXNode.cpp:59-67` — if avatar doesn't cast to `AAuraCharacterBase`, logs warning and returns `Success` without playing any FX
- **Impact**: Visual effects silently dropped for non-Aura characters

#### L18. `HitscanTraceNode` — `DeathImpulse` uses trace direction but `KnockbackForce` uses `UpVector` — UNFIXED
- `HitscanTraceNode.cpp:94` uses `Direction * DeathImpulseMagnitude` (toward target)
- `HitscanTraceNode.cpp:96` uses `FVector::UpVector * KnockbackForceMagnitude` (straight up)
- **Impact**: Death impulse and knockback force use different directions for the same hitscan ability

#### L19. `ApplyDamageNode` always hardcodes `bIsRadialDamage = false` — UNFIXED
- `ApplyDamageNode.cpp:69` — radial damage parameters hardcoded to zero. No XML attribute to enable radial damage
- **Impact**: Cannot create radial damage abilities using the `ApplyDamage` node

#### L20. `CauseDamageNode` uses `GetAbilitySystemComponentFromActorInfo()` while `ApplyDamageNode` uses `Ctx.ASC` — UNFIXED
- `CauseDamageNode.cpp:44` — `SourceASC = DataAbility->GetAbilitySystemComponentFromActorInfo()`
- `ApplyDamageNode.cpp:54` — `SourceASC = Ctx.ASC`
- **Impact**: Should be the same ASC, but using different access patterns could lead to subtle differences

#### L21. Projectiles hardcode `TargetAbilitySystemComponent = nullptr` — UNFIXED
- `SpawnProjectileNode.cpp:97` and `SpawnProjectilesNode.cpp:123` set `TargetAbilitySystemComponent = nullptr`
- **Impact**: Projectiles must resolve the target ASC themselves on hit. If a projectile hits an actor without an ASC, the damage effect would fail silently

#### L22. `WaitForTargetDataTask` has no `OnStart` validation beyond null check — UNFIXED
- `WaitForTargetDataNode.cpp:23-28` — after `CreateTargetDataUnderMouse` returns non-null, no validation that the task is properly initialized or that `ValidData` delegate can actually fire
- **Impact**: If task creation fails silently, the ability would hang

#### L23. No network replication awareness in graph execution — OBSERVATION (not a clear bug)
- Graph execution in `DataAbility` doesn't have explicit client/server replication logic. Async tasks execute on both client and server
- **Impact**: Could cause duplicated effects if not properly replicated by the base `UGameplayAbility` system

#### L24. `SequenceNode::OnExit` resets `ActiveChildIndex` but tasks are not cleaned up for reuse — CONFIRMED (as designed)
- `SequenceNode.cpp:59` — `ActiveChildIndex = 0` is reset. Comment says "tasks are recreated per activation so a fresh index is correct regardless"
- Correct for current architecture (new task objects per activation), but would break if sequence tasks were ever reused

---

## Fixed Issues (from GE rewrite session)

These issues were found and fixed during the GameplayEffect data-driven rewrite (2026-07-29):

#### F1. `GE_Damage` UAsset dependency — FIXED
- `DamageEffectClass` was null when XML omitted `effectClass`, causing FireBolt to silently deal zero damage
- **Fix**: `UAuraDamageGameplayEffect` (C++) with `ExecCalc_Damage`. `DamageEffectClass` defaults to it. `ApplyDamageEffect` and `CauseDamageNode` both fall back to it when null. `FireGun.xml` `effectClass` attribute removed.

#### F2. All attribute init GE UAssets — FIXED
- `PrimaryAttributes_SetByCaller`, `SecondaryAttributes`, `SecondaryAttributes_Infinite`, `VitalAttributes` BPs replaced by `UAuraAttributeGameplayEffect` / `UAuraAttributeGameplayEffect_Infinite` (C++)
- `InitializeDefaultAttributes`, `InitializeDefaultAttributesForRole`, `InitializeDefaultAttributesFromSaveData` all updated to use C++ GEs + `GameplayEffects.json`

#### F3. All pickup/buff GE UAssets — FIXED
- `InstantGameplayEffectClass`, `DurationGameplayEffectClass`, `InfiniteGameplayEffectClass` on `AuraEffectActor` now have data-driven alternatives: `InstantEffectName`, `DurationEffectName`, `InfiniteEffectName` (FString fields indexing into `GameplayEffects.json`)
- `UAuraPickupGameplayEffect` (C++) applies Health/Mana via SetByCaller from JSON config

#### F4. Missing `Attributes.Vital.Health` / `Attributes.Vital.Mana` tags — FIXED
- Added to `AuraGameplayTags.cpp` / `.h` for direct Health/Mana modification by pickup GEs

#### F5. Web editor lacked GE config editing — FIXED
- New "GE Config" tab added to the web editor with sections for Secondary/Vital Attributes, Resistances, and Pickup Effects
- Server endpoints `/load-ge-config` and `/save-ge-config` added to `ability_graph_server.py`
- `ability-data.js` updated with `loadGEConfig()` / `saveGEConfig()` helpers

---

## Summary

| Category | Count | Fixed | Unfixed |
|---|---|---|---|
| Critical (infrastructure/security) | 3 | 0 | 3 |
| High (logic bugs) | 3 | 0 | 3 |
| Medium (dead code/unused) | 8 | 0 | 8 |
| Low (code quality) | 13 | 0 | 13 |
| Fixed (from GE rewrite) | 5 | 5 | 0 |
| **Total** | **32** | **5** | **27** |

**Top priority unfixed**: C1 (git hygiene), C2 (server security), C3 (SourceObject replication), H1 (PlayMontage delegates not wired)