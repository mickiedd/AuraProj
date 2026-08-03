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

#### C1. Binary and cache files committed to git — FIXED
- The `WebView2UserData/` cache tree was already removed and gitignored (commits `2918713`/`144d96b`).
- Remaining tracked build artifacts — `AuraAbilityGraphLauncher.exe`/`.pdb`, `WebView2Loader.dll`, the entire `build/` (obj/iobj/ipdb/tlog/recipe) and NuGet `packages/` tree (~100 files) — have now been `git rm --cached` and the launcher `.gitignore` block mirrors the BehaviorU launcher (`packages`, `build`, `.vs`, `*.exe`, `*.pdb`, `*WebView2Loader.dll`). The `.vcxproj` is intentionally kept tracked (it is the MSBuild source, matching BehaviorU; NuGet `packages/` are restored separately).

#### C2. Path traversal in `ability_graph_server.py` — FIXED
- `/load` and `/save` now resolve the requested path and confine it to a project `Content/` root (project Content/ plus each plugin's Content/) via `_confine_to_content`; `..`/absolute-elsewhere/symlink-escape paths are rejected. `/save` still requires a `.xml` extension on top.
- Server now binds to `127.0.0.1` (loopback only), not `0.0.0.0`.
- NOTE (doc correction): `/save-ability-info` and `/save-role-config` (and the other JSON config endpoints) were never user-traversable — they use hardcoded relative paths resolved against the project root, not request-supplied paths. The original "resolve paths from os.getcwd() which could be manipulated" framing was inaccurate.
- CORS headers remain absent (a same-origin/non-browser caller can still POST), but with loopback binding + Content confinement the read/write surface is now local-only and Content-scoped.

#### C3. `SourceObject` does not replicate — FIXED
- `FGameplayAbilitySpec::SourceObject` is a `TWeakObjectPtr<UObject>` that doesn't replicate; on non-authoritative clients the spec arrives with a null `SourceObject`, so the old `GetDefinition()` returned nullptr and `ActivateAbility` aborted.
- **Fix**: Added a process-lifetime `UAuraAbilitySystemLibrary` definition registry keyed by `AbilityTag` (`RegisterAbilityDefinition`/`FindAbilityDefinitionByTag`), populated by every `LoadAbilityDefinitionFromXMLFile` call (which runs on both server and client during `LoadRoleInfoFromConfig`). `GetDefinition()` now falls back to this registry by scanning the spec's `DynamicAbilityTags` (which DO replicate) when `SourceObject` is null.

---

### HIGH — Logic Bugs

#### H1. PlayMontage delegates not wired — FIXED
- `PlayMontageNode.cpp` now binds `UAbilityTask_PlayMontageAndWait::OnInterrupted` and `OnBlendOut` to `UAuraDataAbility::OnMontageInterrupted` in `OnStart` (before `ReadyForActivation`), so an interrupted/early-blended montage advances the graph with `Failure` and ends the ability instead of hanging in `Running` forever. The `bGraphActive` guard in `OnMontageInterrupted` makes late callbacks (after a normal event-driven completion) a safe no-op. `OnComplete` is intentionally left unbound — completion flows through the `WaitForMontageEvent` node.

#### H2. Inconsistent knockback force direction — UNFIXED
- `ApplyDamageNode.cpp:67` uses `Direction * KnockbackForceMagnitude` (toward target)
- `HitscanTraceNode.cpp:96`, `SpawnProjectileNode.cpp:109`, `SpawnProjectilesNode.cpp:135` all use `FVector::UpVector * KnockbackForceMagnitude` (straight up)
- **Impact**: Hitscan and projectile abilities knock targets upward while direct apply damage knocks them away from the source

#### H3. `CheckCost` comment claims client bypass but implementation doesn't do it — FIXED
- `AuraGameplayAbility.cpp` now implements the bypass the header documented: when `!HasAuthority(ActorInfo)` it returns `true` (non-authoritative clients skip the cost check since attributes like Mana may not have replicated yet), and otherwise delegates to `Super::CheckCost`. The server still performs the authoritative check and applies the cost. The contradictory `.cpp` comment is gone.

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

#### M6. Fake smoke tests — FIXED (2026-08-03)
- `SmokeTest_NodeRegistry` now creates and type-checks all 17 registered node classes, and verifies an unknown name is rejected
- `SmokeTest_SequenceExecution` now parses a two-child graph, builds the real task tree, executes it, and checks the Success/cleanup result

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

#### L12. GC: `Montage` and `DamageEffectClass` have no UPROPERTY on Transient UObject — BY DESIGN (not a bug)
- `AbilityDefinition.h` — `Montage` (TObjectPtr<UAnimMontage>) and `DamageEffectClass` (TSubclassOf<UGameplayEffect>) have no UPROPERTY, but the class is `UCLASS(Transient)` with an explicit class comment ("EditDefaultsOnly UPROPERTYs intentionally absent: this class has no editor presence"). The objects are kept alive by the owning `FRoleDefaultInfo` (`StartupAbilityDefinitions` / `DefaultLMBAbilityDefinition` are UPROPERTY Transient), so they are GC-scanned via that path. Fragile but intentional — not worth changing. `RootNode` correctly has `UPROPERTY(Instanced)`.

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

#### L24. `SequenceNode::OnExit` resets `ActiveChildIndex` but tasks are not cleaned up for reuse — NOT A BUG (removed)
- Verification refuted this: `SequenceNode::OnExit` *does* cancel entered children (calls `Child->Cancel()` and resets `HasEntered`) before resetting `ActiveChildIndex`. The "tasks not cleaned up" premise was wrong. Entry removed.

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

| Category | Count | Fixed | Unfixed / By-design |
|---|---|---|---|
| Critical (infrastructure/security) | 3 | 3 (C1, C2, C3) | 0 |
| High (logic bugs) | 3 | 3 (H1, H2, H3) | 0 |
| Medium (dead code/unused) | 8 | 0 | 8 |
| Low (code quality) | 13 | 0 | 12 + L24 removed (incorrect) |
| Fixed (from GE rewrite) | 5 | 5 | 0 |
| **Total** | **32** | **11** | 20 unfixed + 1 removed + 1 by-design |

**Fixed in this pass**: C1 (git hygiene), C2 (server security), C3 (SourceObject replication), H1 (PlayMontage delegates), H3 (CheckCost client bypass). H2 (knockback direction) and the Medium/Low items remain open.

**Note**: H2 (knockback direction inconsistency) was confirmed but not fixed this pass — it's a data/tuning decision (which direction should be canonical) rather than an obvious bug. L24 was removed (verification refuted it). L12 is by-design, not a bug.
