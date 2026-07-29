# Ability Logic Issues — AuraProj

Analysis date: 2026-07-29
Scope: `Plugins/AuraAbilityGraph` and `Source/Aura/AbilitySystem`

## Architecture Overview

The ability system is a data-driven, XML-parsed, node-graph architecture:

1. **`UAuraAbilityDefinition`** (`AbilityDefinition.h/cpp`) — Parses XML ability definitions from disk. Contains identity, cost/cooldown, damage, animation, and a graph of action nodes.
2. **`UAuraDataAbility`** (`DataAbility.h/cpp`) — The `UGameplayAbility` subclass that executes the ability graph. Manages lifecycle: `ActivateAbility` → cost/cooldown → build graph → `RootTask->Execute()` → `AdvanceGraph()` on async completions → `EndAbility`.
3. **`UAuraAbilityActionNode`** / **`UAuraAbilityActionTask`** — Base node/task classes. Nodes are defined in XML and instantiated via `FAuraAbilityNodeRegistry`. Tasks execute `OnStart()` and return `EAuraAbilityActionStatus` (Success/Failure/Running).
4. **`UAuraSequenceTask`** (`SequenceNode.cpp`) — Composite node that executes children in order. Advances `ActiveChildIndex` on success, fails on first failure, returns `Running` if any child is async.
5. **Action Nodes** — `ApplyDamage`, `CauseDamage`, `HitscanTrace`, `SpawnProjectile`, `SpawnProjectiles`, `PlayMontage`, `WaitForMontageEvent`, `WaitForTargetData`, `FaceTarget`, `MulticastGunFX`.
6. **`UAuraAbilitySystemLibrary`** — Core library with `ApplyDamageEffect()` that creates a GE spec with all damage parameters and applies it to the target.
7. **`FDamageEffectParams`** (`AuraAbilityTypes.h`) — Struct holding all damage parameters for `ApplyDamageEffect`.
8. **`FAuraGameplayEffectContext`** (`AuraAbilityTypes.h`) — Custom `FGameplayEffectContext` subclass with additional fields (crit, block, debuff, radial, etc.).

## Issues Found (Validated 2026-07-29)

### HIGH Severity

#### H1. Inconsistent Knockback Force Direction — CONFIRMED
- `ApplyDamageNode.cpp:67` uses `Direction * KnockbackForceMagnitude` (toward target)
- `HitscanTraceNode.cpp:96`, `SpawnProjectileNode.cpp:109`, `SpawnProjectilesNode.cpp:135` all use `FVector::UpVector * KnockbackForceMagnitude` (straight up)
- **Impact**: Hitscan and projectile abilities knock targets upward while direct apply damage knocks them away from the source. This is likely unintentional inconsistency.
- **Validation**: Direct code comparison across all four files confirms the inconsistency.

#### H2. `CheckCost` Comment Claims Client Bypass But Implementation Doesn't Do It — CONFIRMED
- `AuraGameplayAbility.h:26-28` comment says "Skip the cost check on non-authoritative clients — attributes (e.g. Mana) may not have replicated yet."
- `AuraGameplayAbility.cpp:9-13` just delegates to `Super::CheckCost` with no client-side bypass.
- **Impact**: On clients, mana cost check may fail if mana hasn't replicated yet, preventing ability activation even though the server would allow it.
- **Validation**: The comment and implementation are directly contradictory. The `CheckCost` override adds no client-side logic.

### MEDIUM Severity

#### M3. `PlayMontageTask::OnStart` Returns `Success` When No Montage — CONFIRMED
- `PlayMontageNode.cpp:29` — returns `EAuraAbilityActionStatus::Success` instead of `Failure` when `Definition->Montage` is null.
- **Impact**: Missing montage assets are silently skipped rather than signaling an error, making debugging difficult.
- **Validation**: Direct code reading confirms the `Success` return on the null montage path.

#### M4. `WaitForMontageEventTask` — No Validation of EventTag — PARTIALLY CONFIRMED
- `WaitForMontageEventNode.cpp:38-46` — If neither the node's `EventTag` nor the definition's `MontageEventTag` is set, `EventTag` will be invalid. `UAbilityTask_WaitGameplayEvent::WaitGameplayEvent` with an invalid tag may never fire or may behave unpredictably.
- **Impact**: Ability graph could hang indefinitely waiting for an event that never fires.
- **Validation**: The code does fall through to use `Definition->MontageEventTag` if the node's `EventTag` is invalid. However, both XML definitions (`FireBolt.xml`, `FireGun.xml`) always set `eventTag` on the `<montage>` element, and the `WaitForMontageEventNode` in the XML graph also explicitly sets `EventTag` property. The issue is valid as a defensive concern but may not be triggered with current XML definitions.

#### M5. Two Separate Damage Application Paths — CONFIRMED
- `ApplyDamageNode` uses `UAuraAbilitySystemLibrary::ApplyDamageEffect(Params)` which internally creates its own GE spec and context.
- `CauseDamageNode` uses `DataAbility->MakeOutgoingGameplayEffectSpec` + `SourceASC->ApplyGameplayEffectSpecToTarget` directly.
- **Impact**: These two paths could produce different results if the GE context setup differs between them (e.g., source object, effect context handle).
- **Validation**: Both code paths were read and confirmed. `ApplyDamageEffect` creates its own `FGameplayEffectContextHandle` internally (line 989-990 of `AuraAbilitySystemLibrary.cpp`), while `CauseDamageNode` uses `MakeOutgoingGameplayEffectSpec` which also creates a context. The contexts may differ in source object handling.

#### M6. `WaitForMontageEventTask::OnEventReceived` Bypasses `OnMontageEventReceived` — CONFIRMED
- `WaitForMontageEventNode.cpp:70-79` — `OnEventReceived` calls `AdvanceGraph` directly, bypassing `DataAbility::OnMontageEventReceived`.
- Meanwhile, `DataAbility::OnMontageEventReceived` (line 312-327) also calls `AdvanceGraph`.
- **Impact**: If both fire for the same event (e.g., from Blueprint and from the delegate), `AdvanceGraph` is called twice. The second call is a no-op since `bGraphActive` would be false after the first call ends the ability, but it's still a code smell and potential source of bugs.
- **Validation**: Both call sites were confirmed. `OnEventReceived` is the delegate callback from `UAbilityTask_WaitGameplayEvent`, while `OnMontageEventReceived` is a `UFUNCTION` likely called from Blueprint.

#### M7. `WaitForTargetDataTask::OnValidData` No Active-Graph Check — CONFIRMED
- `WaitForTargetDataNode.cpp:45-53` — `OnValidData` calls `DataAbility->OnTargetDataReady()` without checking `bGraphActive`.
- **Impact**: While `OnTargetDataReady` and `AdvanceGraph` do check `bGraphActive`, the `PendingStatus` is set to `Success` unconditionally, which could cause issues if the task is reused or if the ability has already ended.
- **Validation**: Direct code reading confirms `OnValidData` does not check `bGraphActive` before calling `OnTargetDataReady`.

### LOW Severity

#### L8. `CooldownDuration` Doesn't Scale from XML — CONFIRMED
- `AbilityDefinition.cpp:138` — `CooldownDuration.Value = FCString::Atof(*DurationStr)` sets only the base value, ignoring the `ScalableFloat` curve.
- **Impact**: Cooldowns don't scale with ability level from XML. The `CooldownDuration` should be set via a curve or scaled properly for level-based gameplay.
- **Validation**: Direct code reading confirms only `Value` is set, not any curve data.

#### L9. `FDamageEffectParams::WorldContextObject` Never Populated — CONFIRMED
- `AuraAbilityTypes.h:16` — `WorldContextObject` is declared but always `nullptr` when passed from the ability nodes.
- `AuraAbilitySystemLibrary.cpp:984-1009` — `ApplyDamageEffect` never reads `DamageEffectParams.WorldContextObject`.
- **Impact**: Unused field; could be used for world-context-dependent operations if populated.
- **Validation**: Searched all damage node implementations for `WorldContextObject` — none set it. `ApplyDamageEffect` also never reads it.

#### L10. `AbilityDefinitionImportFactory::CanReimport` Always Returns False — CONFIRMED
- `AbilityDefinitionImportFactory.cpp:47-58` — The factory doesn't support reimport, and `SourceFilePath` is not stored on the asset.
- **Impact**: XML changes can't be reimported into an existing asset; the asset must be deleted and re-imported.
- **Validation**: Direct code reading confirms `CanReimport` returns `false` and `Reimport` returns `EReimportResult::Failed`.

#### L11. `MulticastGunFXNode` — No Fallback If Avatar Is Not `AAuraCharacterBase` — CONFIRMED
- `MulticastGunFXNode.cpp:59-67` — If the avatar actor doesn't implement `AAuraCharacterBase`, the function logs a warning and returns `Success` without playing any FX.
- **Impact**: Visual effects are silently dropped for non-Aura characters.
- **Validation**: Direct code reading confirms the `Cast<AAuraCharacterBase>` check with no fallback path.

#### L12. `HitscanTraceNode` — `DeathImpulse` Uses Trace Direction but `KnockbackForce` Uses `UpVector` — CONFIRMED
- `HitscanTraceNode.cpp:94` uses `Direction * DeathImpulseMagnitude` (toward target)
- `HitscanTraceNode.cpp:96` uses `FVector::UpVector * KnockbackForceMagnitude` (straight up)
- **Impact**: Death impulse and knockback force use different directions for the same hitscan ability, which may feel inconsistent.
- **Validation**: Direct code comparison within the same file confirms the inconsistency.

#### L13. `ApplyDamageNode` Always Uses `bIsRadialDamage = false` — CONFIRMED
- `ApplyDamageNode.cpp:69` — Radial damage parameters are hardcoded to zero. There's no XML attribute to enable radial damage for this node type.
- **Impact**: Cannot create radial damage abilities using the `ApplyDamage` node.
- **Validation**: Direct code reading confirms `bIsRadialDamage = false` is hardcoded.

#### L14. `WaitForTargetDataTask` Has No `OnStart` Validation Beyond Null Check — CONFIRMED
- `WaitForTargetDataNode.cpp:23-28` — After `CreateTargetDataUnderMouse` returns non-null, there's no validation that the task is properly initialized or that the `ValidData` delegate can actually fire.
- **Impact**: If the task creation fails silently, the ability would hang waiting for target data that never arrives.
- **Validation**: Direct code reading confirms no additional validation beyond the null check.

#### L15. `SequenceNode::OnExit` Resets `ActiveChildIndex` But Tasks Are Not Cleaned Up for Reuse — CONFIRMED (as designed)
- `SequenceNode.cpp:59` — `ActiveChildIndex = 0` is reset, but the comment says "tasks are recreated per activation so a fresh index is correct regardless."
- **Impact**: This is correct for the current architecture (new task objects per activation), but if sequence tasks are ever reused, the child task state (`HasEntered`, `PendingStatus`) would not be reset.
- **Validation**: Direct code reading confirms the reset is intentional and the architecture creates new task objects per activation.

#### L16. No Network Replication Awareness in Graph Execution — OBSERVATION (not a bug)
- The graph execution in `DataAbility` doesn't have explicit client/server replication logic. Async tasks like `WaitForTargetData` and `WaitForMontageEvent` will execute on both client and server.
- **Impact**: Could cause duplicated effects if not properly replicated by the base `UGameplayAbility` system. The `ActivateAbility` call on the client would also run cost/cooldown checks and graph execution, which should ideally be server-authoritative.
- **Validation**: This is an architectural observation rather than a clear bug. The `UGameplayAbility` base class handles replication, and the graph execution is designed to run on both client and server.

#### L17. `CauseDamageNode` Uses `GetAbilitySystemComponentFromActorInfo()` While `ApplyDamageNode` Uses `Ctx.ASC` — CONFIRMED
- `CauseDamageNode.cpp:44` — `SourceASC = DataAbility->GetAbilitySystemComponentFromActorInfo()`
- `ApplyDamageNode.cpp:54` — `SourceASC = Ctx.ASC` (set from `ActorInfo->AbilitySystemComponent.Get()`)
- **Impact**: These should be the same ASC, but using different access patterns could lead to subtle differences if the context ASC is ever different from the actor info's ASC.
- **Validation**: Both code paths were read and confirmed. Both should resolve to the same ASC in practice.

#### L18. Projectiles Hardcode `TargetAbilitySystemComponent = nullptr` — CONFIRMED
- `SpawnProjectileNode.cpp:97` and `SpawnProjectilesNode.cpp:123` set `TargetAbilitySystemComponent = nullptr`.
- **Impact**: Projectiles that use these params must resolve the target ASC themselves on hit. If a projectile hits an actor without an ASC, the damage effect would fail silently or crash.
- **Validation**: Direct code reading confirms both projectile nodes set `TargetAbilitySystemComponent = nullptr`.

### HIGH Severity

#### H1. Inconsistent Knockback Force Direction
- `ApplyDamageNode.cpp:67` uses `Direction * KnockbackForceMagnitude` (toward target)
- `HitscanTraceNode.cpp:96`, `SpawnProjectileNode.cpp:109`, `SpawnProjectilesNode.cpp:135` all use `FVector::UpVector * KnockbackForceMagnitude` (straight up)
- **Impact**: Hitscan and projectile abilities knock targets upward while direct apply damage knocks them away from the source. This is likely unintentional inconsistency.

#### H2. `CheckCost` Comment Claims Client Bypass But Implementation Doesn't Do It
- `AuraGameplayAbility.h:26-28` comment says "Skip the cost check on non-authoritative clients — attributes (e.g. Mana) may not have replicated yet."
- `AuraGameplayAbility.cpp:9-13` just delegates to `Super::CheckCost` with no client-side bypass.
- **Impact**: On clients, mana cost check may fail if mana hasn't replicated yet, preventing ability activation even though the server would allow it.

### MEDIUM Severity

#### M3. `PlayMontageTask::OnStart` Returns `Success` When No Montage
- `PlayMontageNode.cpp:29` — returns `EAuraAbilityActionStatus::Success` instead of `Failure` when `Definition->Montage` is null.
- **Impact**: Missing montage assets are silently skipped rather than signaling an error, making debugging difficult.

#### M4. `WaitForMontageEventTask` — No Validation of EventTag
- `WaitForMontageEventNode.cpp:38-46` — If neither the node's `EventTag` nor the definition's `MontageEventTag` is set, `EventTag` will be invalid. `UAbilityTask_WaitGameplayEvent::WaitGameplayEvent` with an invalid tag may never fire or may behave unpredictably.
- **Impact**: Ability graph could hang indefinitely waiting for an event that never fires.

#### M5. Two Separate Damage Application Paths
- `ApplyDamageNode` uses `UAuraAbilitySystemLibrary::ApplyDamageEffect(Params)` which internally creates its own GE spec and context.
- `CauseDamageNode` uses `DataAbility->MakeOutgoingGameplayEffectSpec` + `SourceASC->ApplyGameplayEffectSpecToTarget` directly.
- **Impact**: These two paths could produce different results if the GE context setup differs between them (e.g., source object, effect context handle).

#### M6. `WaitForMontageEventTask::OnEventReceived` Bypasses `OnMontageEventReceived`
- `WaitForMontageEventNode.cpp:70-79` — `OnEventReceived` calls `AdvanceGraph` directly, bypassing `DataAbility::OnMontageEventReceived`.
- Meanwhile, `DataAbility::OnMontageEventReceived` (line 312-327) also calls `AdvanceGraph`.
- **Impact**: If both fire for the same event (e.g., from Blueprint and from the delegate), `AdvanceGraph` is called twice. The second call is a no-op since `bGraphActive` would be false after the first call ends the ability, but it's still a code smell and potential source of bugs.

#### M7. `WaitForTargetDataTask::OnValidData` No Active-Graph Check
- `WaitForTargetDataNode.cpp:45-53` — `OnValidData` calls `DataAbility->OnTargetDataReady()` without checking `bGraphActive`.
- **Impact**: While `OnTargetDataReady` and `AdvanceGraph` do check `bGraphActive`, the `PendingStatus` is set to `Success` unconditionally, which could cause issues if the task is reused or if the ability has already ended.

### LOW Severity

#### L8. `CooldownDuration` Doesn't Scale from XML
- `AbilityDefinition.cpp:138` — `CooldownDuration.Value = FCString::Atof(*DurationStr)` sets only the base value, ignoring the `ScalableFloat` curve.
- **Impact**: Cooldowns don't scale with ability level from XML. The `CooldownDuration` should be set via a curve or scaled properly for level-based gameplay.

#### L9. `FDamageEffectParams::WorldContextObject` Never Populated
- `AuraAbilityTypes.h:16` — `WorldContextObject` is declared but always `nullptr` when passed from the ability nodes.
- `AuraAbilitySystemLibrary.cpp:984-1009` — `ApplyDamageEffect` never reads `DamageEffectParams.WorldContextObject`.
- **Impact**: Unused field; could be used for world-context-dependent operations if populated.

#### L10. `AbilityDefinitionImportFactory::CanReimport` Always Returns False
- `AbilityDefinitionImportFactory.cpp:47-58` — The factory doesn't support reimport, and `SourceFilePath` is not stored on the asset.
- **Impact**: XML changes can't be reimported into an existing asset; the asset must be deleted and re-imported.

#### L11. `MulticastGunFXNode` — No Fallback If Avatar Is Not `AAuraCharacterBase`
- `MulticastGunFXNode.cpp:59-67` — If the avatar actor doesn't implement `AAuraCharacterBase`, the function logs a warning and returns `Success` without playing any FX.
- **Impact**: Visual effects are silently dropped for non-Aura characters.

#### L12. `HitscanTraceNode` — `DeathImpulse` Uses Trace Direction but `KnockbackForce` Uses `UpVector`
- `HitscanTraceNode.cpp:94` uses `Direction * DeathImpulseMagnitude` (toward target)
- `HitscanTraceNode.cpp:96` uses `FVector::UpVector * KnockbackForceMagnitude` (straight up)
- **Impact**: Death impulse and knockback force use different directions for the same hitscan ability, which may feel inconsistent.

#### L13. `ApplyDamageNode` Always Uses `bIsRadialDamage = false`
- `ApplyDamageNode.cpp:69` — Radial damage parameters are hardcoded to zero. There's no XML attribute to enable radial damage for this node type.
- **Impact**: Cannot create radial damage abilities using the `ApplyDamage` node.

#### L14. `WaitForTargetDataTask` Has No `OnStart` Validation Beyond Null Check
- `WaitForTargetDataNode.cpp:23-28` — After `CreateTargetDataUnderMouse` returns non-null, there's no validation that the task is properly initialized or that the `ValidData` delegate can actually fire.
- **Impact**: If the task creation fails silently, the ability would hang waiting for target data that never arrives.

#### L15. `SequenceNode::OnExit` Resets `ActiveChildIndex` But Tasks Are Not Cleaned Up for Reuse
- `SequenceNode.cpp:59` — `ActiveChildIndex = 0` is reset, but the comment says "tasks are recreated per activation so a fresh index is correct regardless."
- **Impact**: This is correct for the current architecture (new task objects per activation), but if sequence tasks are ever reused, the child task state (`HasEntered`, `PendingStatus`) would not be reset.

#### L16. No Network Replication Awareness in Graph Execution
- The graph execution in `DataAbility` doesn't have explicit client/server replication logic. Async tasks like `WaitForTargetData` and `WaitForMontageEvent` will execute on both client and server.
- **Impact**: Could cause duplicated effects if not properly replicated by the base `UGameplayAbility` system. The `ActivateAbility` call on the client would also run cost/cooldown checks and graph execution, which should ideally be server-authoritative.

#### L17. `CauseDamageNode` Uses `GetAbilitySystemComponentFromActorInfo()` While `ApplyDamageNode` Uses `Ctx.ASC`
- `CauseDamageNode.cpp:44` — `SourceASC = DataAbility->GetAbilitySystemComponentFromActorInfo()`
- `ApplyDamageNode.cpp:54` — `SourceASC = Ctx.ASC` (set from `ActorInfo->AbilitySystemComponent.Get()`)
- **Impact**: These should be the same ASC, but using different access patterns could lead to subtle differences if the context ASC is ever different from the actor info's ASC.

#### L18. `ApplyDamageNode` and `CauseDamageNode` Both Hardcode `TargetAbilitySystemComponent = nullptr` for Projectiles
- `SpawnProjectileNode.cpp:97` and `SpawnProjectilesNode.cpp:123` set `TargetAbilitySystemComponent = nullptr`.
- **Impact**: Projectiles that use these params must resolve the target ASC themselves on hit. If a projectile hits an actor without an ASC, the damage effect would fail silently or crash.