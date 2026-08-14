// Copyright Druid Mechanics

#include "DataAbility.h"
#include "AbilityDefinition.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "AbilitySystem/Data/RoleInfo.h"
#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Character/AuraCharacterBase.h"
#include "Interaction/CombatInterface.h"
#include "Nodes/AbilityActionTask.h"
#include "Nodes/Composites/SequenceNode.h"
#include "AuraCooldownGameplayEffect.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"

UAuraDataAbility::UAuraDataAbility()
{
    SharedCostGE = UAuraManaCostGameplayEffect::StaticClass();
    SharedCooldownGE = UAuraCooldownGameplayEffect::StaticClass();

    // The ability stores per-activation graph state on itself (RootTask, bGraphActive,
    // PersistentCtx, Pending*Task). With the default NonInstanced policy ActivateAbility
    // runs on the CDO (Default__UAuraDataAbility) — a single object shared by EVERY
    // DataAbility spec (FireBolt, ArcaneShards, Electrocute, ...). Overlapping or
    // back-to-back activations overwrite each other's RootTask/bGraphActive, which:
    //   - orphans a still-Running graph (its RootTask pointer is clobbered), so the
    //     ability never ends ("stuck Running" — e.g. ArcaneShards waiting on a montage
    //     event that never fires while a later cast repoints RootTask);
    //   - leaks the orphaned tasks (Outer == CDO, process-lifetime, never GC'd) — the
    //     PIE leak the weak-capture work was chasing;
    //   - makes PIE teardown cancel a stale/wrong RootTask whose World-owned resources
    //     (beam tick timer, Niagara arc) belong to a torn-down PIE world → the
    //     EXCEPTION_ACCESS_VIOLATION reading 0xFFFFFFFFFFFFFFFF in AuraAbilityGraph at
    //     editor shutdown.
    // InstancedPerActor gives each ability spec (per owner ASC) its own instance, so the
    // graph state is isolated per ability+actor and the instance (and its tasks) is
    // destroyed with the actor — no sharing, no leak, no stale-RootTask teardown crash.
    InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

UAuraEnemyAttackDataAbility::UAuraEnemyAttackDataAbility()
{
    FGameplayTagContainer Tags;
    // The plugin CDO can be constructed before project/native gameplay tags are
    // registered. Do not assert during module startup; the definition's dynamic
    // tags still carry the authoritative runtime tags after config loading.
    const FGameplayTag AttackTag = FGameplayTag::RequestGameplayTag(TEXT("Abilities.Attack"), false);
    if (AttackTag.IsValid())
    {
        Tags.AddTag(AttackTag);
    }
    SetAssetTags(Tags);
}

UAuraEnemyHitReactDataAbility::UAuraEnemyHitReactDataAbility()
{
    FGameplayTagContainer Tags;
    const FGameplayTag HitReactTag = FGameplayTag::RequestGameplayTag(TEXT("Effects.HitReact"), false);
    if (HitReactTag.IsValid())
    {
        Tags.AddTag(HitReactTag);
    }
    SetAssetTags(Tags);
}

const UAuraAbilityDefinition* UAuraDataAbility::GetDefinition() const
{
    if (CurrentActorInfo && CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        if (const FGameplayAbilitySpec* Spec = CurrentActorInfo->AbilitySystemComponent->FindAbilitySpecFromHandle(CurrentSpecHandle))
        {
            if (Spec->SourceObject.IsValid())
            {
                return Cast<UAuraAbilityDefinition>(Spec->SourceObject.Get());
            }

            // FGameplayAbilitySpec::SourceObject is a TWeakObjectPtr that does NOT replicate.
            // On non-authoritative clients the spec arrives with a null SourceObject, so the
            // ability resolves through the process-lifetime definition registry keyed by the
            // replicated ability tag. The registry can be empty after a map travel: the login
            // world's RoleInfo owner is gone and its weak entries have been garbage-collected.
            const auto FindRegisteredDefinition = [Spec]() -> const UAuraAbilityDefinition*
            {
                for (const FGameplayTag& Tag : Spec->GetDynamicSpecSourceTags())
                {
                    if (const UAuraAbilityDefinition* Def = UAuraAbilitySystemLibrary::FindAbilityDefinitionByTag(Tag))
                    {
                        return Def;
                    }
                }
                return nullptr;
            };

            if (const UAuraAbilityDefinition* Def = FindRegisteredDefinition())
            {
                return Def;
            }

            // Rebuild the client-side RoleInfo cache on demand. This is important for a
            // network client that travels Login -> Loading -> gameplay: the definition
            // registry was populated while Login was alive, then its transient definitions
            // were collected when that world was torn down. Loading RoleConfig here roots the
            // definitions in the process-lifetime client cache and re-registers them before
            // the predicted activation applies any ability-side work.
            const UObject* WorldContext = CurrentActorInfo->AvatarActor.Get();
            if (!WorldContext)
            {
                WorldContext = CurrentActorInfo->OwnerActor.Get();
            }
            if (WorldContext)
            {
                const auto FindDefinitionInRoleInfo = [Spec](const URoleInfo* RoleInfo) -> const UAuraAbilityDefinition*
                {
                    if (!RoleInfo)
                    {
                        return nullptr;
                    }

                    for (const TPair<FName, FRoleDefaultInfo>& RolePair : RoleInfo->RoleInformation)
                    {
                        const FRoleDefaultInfo& Role = RolePair.Value;
                        const auto FindInObjects = [Spec](const TArray<TObjectPtr<UObject>>& Objects) -> const UAuraAbilityDefinition*
                        {
                            for (const FGameplayTag& Tag : Spec->GetDynamicSpecSourceTags())
                            {
                                for (const TObjectPtr<UObject>& Object : Objects)
                                {
                                    const UAuraAbilityDefinition* Def = Cast<UAuraAbilityDefinition>(Object.Get());
                                    if (Def && Def->AbilityTag == Tag)
                                    {
                                        return Def;
                                    }
                                }
                            }
                            return nullptr;
                        };

                        if (const UAuraAbilityDefinition* Def = FindInObjects(Role.StartupAbilityDefinitions))
                        {
                            return Def;
                        }
                        if (const UAuraAbilityDefinition* Def = FindInObjects(Role.StartupPassiveAbilityDefinitions))
                        {
                            return Def;
                        }

                        if (const UAuraAbilityDefinition* Def = Cast<UAuraAbilityDefinition>(Role.DefaultLMBAbilityDefinition.Get()))
                        {
                            for (const FGameplayTag& Tag : Spec->GetDynamicSpecSourceTags())
                            {
                                if (Def->AbilityTag == Tag)
                                {
                                    return Def;
                                }
                            }
                        }
                    }
                    return nullptr;
                };

                const URoleInfo* RoleInfo = UAuraAbilitySystemLibrary::GetRoleInfo(WorldContext);
                if (const UAuraAbilityDefinition* Def = FindRegisteredDefinition())
                {
                    UE_LOG(LogAuraAbilityGraph, Verbose,
                        TEXT("[DataAbility] GetDefinition rebuilt client definition cache for handle=%s"),
                        *CurrentSpecHandle.ToString());
                    return Def;
                }

                // The role cache can still own the definitions even when the weak
                // process registry was cleared during map travel. Recover directly
                // from those rooted objects, then restore the registry for future
                // lookups and other abilities in the same client world.
                if (const UAuraAbilityDefinition* Def = FindDefinitionInRoleInfo(RoleInfo))
                {
                    UAuraAbilitySystemLibrary::RegisterAbilityDefinition(const_cast<UAuraAbilityDefinition*>(Def));
                    UE_LOG(LogAuraAbilityGraph, Verbose,
                        TEXT("[DataAbility] GetDefinition recovered rooted RoleInfo definition for handle=%s"),
                        *CurrentSpecHandle.ToString());
                    return Def;
                }
            }

            UE_LOG(LogAuraAbilityGraph, Warning,
                TEXT("[DataAbility] GetDefinition failed to resolve replicated definition for handle=%s"),
                *CurrentSpecHandle.ToString());
        }
    }
    return nullptr;
}

const FGameplayTagContainer* UAuraDataAbility::GetCooldownTags() const
{
    CachedCooldownTags.Reset();
    if (const UAuraAbilityDefinition* Definition = GetDefinition())
    {
        CachedCooldownTags.AddTag(Definition->CooldownTag);
    }
    return &CachedCooldownTags;
}

void UAuraDataAbility::ActivateAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, const FGameplayEventData* TriggerEventData)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] ActivateAbility START handle=%s"), *Handle.ToString());
    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] ActivateAbility ABORT: invalid ActorInfo/ASC"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    FGameplayTagContainer RelevantTags;
    if (!CheckCost(Handle, ActorInfo, &RelevantTags))
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] ActivateAbility ABORT: CheckCost failed"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    if (!CheckCooldown(Handle, ActorInfo, &RelevantTags))
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] ActivateAbility ABORT: CheckCooldown failed"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, false);
        return;
    }

    ApplyCost(Handle, ActorInfo, ActivationInfo);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] ActivateAbility ApplyCost done"));
    ApplyCooldown(Handle, ActorInfo, ActivationInfo);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] ActivateAbility ApplyCooldown done"));

    if (const UAuraAbilityDefinition* Definition = GetDefinition())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] Activating %s (RootNode=%s)"), *Definition->AbilityTag.ToString(), Definition->RootNode ? *Definition->RootNode->NodeClassName : TEXT("null"));
    }
    else
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] ActivateAbility WARNING: no definition found on spec"));
    }

    PersistentCtx.ASC = ActorInfo->AbilitySystemComponent.Get();
    PersistentCtx.AvatarActor = ActorInfo->AvatarActor.Get();
    PersistentCtx.SpecHandle = Handle;
    PersistentCtx.Definition = nullptr;
    PersistentCtx.NodeDef = nullptr;
    PersistentCtx.TargetDataHandle.Clear();
    PersistentCtx.CursorHit = FHitResult();
    PersistentCtx.BeamState.Reset();
    PersistentCtx.bStopCurrentTimedLoop = false;
    bWaitingForAuthoritativeEnd = false;

    if (const UAuraAbilityDefinition* Definition = GetDefinition())
    {
        PersistentCtx.NodeDef = Definition->RootNode;
        PersistentCtx.Definition = Definition;
    }

    if (!PersistentCtx.NodeDef)
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[DataAbility] ActivateAbility ABORT: RootNode is null"));
        EndAbility(Handle, ActorInfo, ActivationInfo, true, true);
        return;
    }

    RootTask = NewObject<UAuraSequenceTask>(this);
    RootTask->Init(const_cast<UAuraAbilityActionNode*>(PersistentCtx.NodeDef), this);
    RootTask->ParentTask = nullptr;

    bGraphActive = true;

    EAuraAbilityActionStatus Status = RootTask->Execute(PersistentCtx);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] RootTask initial Execute returned %s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
    if (Status != EAuraAbilityActionStatus::Running)
    {
        if (Status == EAuraAbilityActionStatus::Success && !ShouldEndAfterGraphCompletion(ActorInfo->IsNetAuthority()))
        {
            bGraphActive = false;
            bWaitingForAuthoritativeEnd = true;
            RootTask = nullptr;
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] Graph completed on non-authority client; waiting for authoritative server end"));
            return;
        }

        EndAbility(Handle, ActorInfo, ActivationInfo, true, Status == EAuraAbilityActionStatus::Failure);
        return;
    }
}

void UAuraDataAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] EndAbility START handle=%s bWasCancelled=%s"), *Handle.ToString(), bWasCancelled ? TEXT("true") : TEXT("false"));

    // Re-entrancy guard: ending a Pending montage/event task below can fire
    // OnInterrupted/OnMontageInterrupted synchronously, which re-enters EndAbility via
    // AdvanceGraph(Failure). The recursive call must do nothing — the ability is
    // already being torn down.
    if (bIsEndingAbility)
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] EndAbility re-entrant call ignored"));
        return;
    }
    bIsEndingAbility = true;

    // Mark the graph inactive BEFORE tearing it down so any callback that fires
    // synchronously during cleanup (OnMontageInterrupted, OnTargetDataReady, a tick
    // timer firing into AdvanceGraph) is a no-op instead of re-advancing/cancelling a
    // half-destroyed graph.
    bGraphActive = false;
    bWaitingForAuthoritativeEnd = false;

    if (!ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] EndAbility: stale ActorInfo, skipping graph cleanup"));
        bIsEndingAbility = false;
        Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
        return;
    }

    FAuraAbilityExecutionContext Ctx;
    Ctx.ASC = ActorInfo->AbilitySystemComponent.Get();
    Ctx.AvatarActor = ActorInfo->AvatarActor.Get();
    Ctx.SpecHandle = Handle;

    if (IsValid(RootTask))
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] EndAbility cancelling RootTask"));
        FAuraAbilityExecutionContext CancelCtx;
        CancelCtx.ASC = ActorInfo->AbilitySystemComponent.Get();
        CancelCtx.AvatarActor = ActorInfo->AvatarActor.Get();
        CancelCtx.SpecHandle = Handle;
        CancelCtx.BeamState = PersistentCtx.BeamState;

        RootTask->Cancel(CancelCtx);
        RootTask = nullptr;
    }

    if (PendingTargetDataTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] EndAbility ending PendingTargetDataTask"));
        PendingTargetDataTask->EndTask();
        PendingTargetDataTask.Reset();
    }
    if (PendingMontageTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] EndAbility ending PendingMontageTask"));
        PendingMontageTask->EndTask();
        PendingMontageTask.Reset();
    }
    if (PendingMontageEventTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] EndAbility ending PendingMontageEventTask"));
        PendingMontageEventTask->EndTask();
        PendingMontageEventTask.Reset();
    }

    PersistentCtx.TargetDataHandle.Clear();
    PersistentCtx.CursorHit = FHitResult();
    PersistentCtx.BeamState.Reset();
    PersistentCtx.bStopCurrentTimedLoop = false;
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] EndAbility END"));
    bIsEndingAbility = false;
    Super::EndAbility(Handle, ActorInfo, ActivationInfo, bReplicateEndAbility, bWasCancelled);
}

bool UAuraDataAbility::CheckCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, FGameplayTagContainer* OptionalRelevantTags) const
{
    if (!GetDefinition() || GetDefinition()->ManaCost <= 0.f)
    {
        return true;
    }

    if (ActorInfo && ActorInfo->AbilitySystemComponent.IsValid())
    {
        bool bFound = false;
        const float CurrentMana = ActorInfo->AbilitySystemComponent->GetGameplayAttributeValue(UAuraAttributeSet::GetManaAttribute(), bFound);
        if (bFound && CurrentMana < GetDefinition()->ManaCost)
        {
            return false;
        }
    }

    return true;
}

void UAuraDataAbility::ApplyCost(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    if (!GetDefinition() || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid() || !SharedCostGE)
    {
        return;
    }

    if (GetDefinition()->ManaCost <= 0.f)
    {
        return;
    }

    FGameplayEffectSpecHandle SpecHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(SharedCostGE, GetAbilityLevel(), MakeEffectContext(Handle, ActorInfo));
    if (SpecHandle.IsValid())
    {
        // UAuraManaCostGameplayEffect's modifier uses SetByCaller.DataName = "Abilities.Cost.Mana"
        // (an FName, not an FGameplayTag), so it looks up the spec's SetByCallerNameMagnitudes map.
        // Use the FName-based assign to match — the tag-based assign writes a different map and the
        // magnitude would never bind (leaving the cost at 0, so mana was never consumed).
        UAbilitySystemBlueprintLibrary::AssignSetByCallerMagnitude(SpecHandle, FName("Abilities.Cost.Mana"), -GetDefinition()->ManaCost);
        ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] ApplyCost mana cost=%.1f applied via GE"), GetDefinition()->ManaCost);
    }
}

void UAuraDataAbility::ApplyCooldown(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo) const
{
    if (!GetDefinition() || !SharedCooldownGE || !ActorInfo || !ActorInfo->AbilitySystemComponent.IsValid())
    {
        return;
    }

    FGameplayEffectSpecHandle SpecHandle = ActorInfo->AbilitySystemComponent->MakeOutgoingSpec(SharedCooldownGE, GetAbilityLevel(), MakeEffectContext(Handle, ActorInfo));
    if (!SpecHandle.IsValid()) return;

    if (FGameplayEffectSpec* Spec = SpecHandle.Data.Get())
    {
        const float Duration = GetDefinition()->CooldownDuration.GetValueAtLevel(GetAbilityLevel());
        Spec->SetDuration(Duration, false);
        Spec->DynamicGrantedTags.AddTag(GetDefinition()->CooldownTag);
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] ApplyCooldown duration=%.1f tag=%s"), Duration, *GetDefinition()->CooldownTag.ToString());
    }

    ActorInfo->AbilitySystemComponent->ApplyGameplayEffectSpecToSelf(*SpecHandle.Data.Get());
}

void UAuraDataAbility::AdvanceGraph(EAuraAbilityActionStatus ChildStatus)
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[DataAbility] AdvanceGraph called with ChildStatus=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(ChildStatus));
    if (!bGraphActive)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] AdvanceGraph ignored: graph not active"));
        return;
    }

    if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] AdvanceGraph aborted: stale ActorInfo"));
        return;
    }

    PersistentCtx.ASC = CurrentActorInfo->AbilitySystemComponent.Get();
    PersistentCtx.AvatarActor = CurrentActorInfo->AvatarActor.Get();
    PersistentCtx.SpecHandle = CurrentSpecHandle;

    // A forced Failure (e.g. the cast montage was interrupted) means the graph cannot
    // continue — end the ability as cancelled. Re-executing the RootTask here would
    // just re-run the currently-Running child (which has no PendingStatus yet) and
    // return Running again, swallowing the cancel signal and leaving the ability hung
    // forever — which is exactly the "second press does nothing" symptom. End now.
    if (ChildStatus == EAuraAbilityActionStatus::Failure)
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] AdvanceGraph forced Failure, ending ability as cancelled"));
        const FGameplayAbilityActivationInfo& ActivationInfo = GetCurrentActivationInfo();
        EndAbility(CurrentSpecHandle, CurrentActorInfo, ActivationInfo, true, true);
        return;
    }

    if (RootTask)
    {
        EAuraAbilityActionStatus Status = RootTask->Execute(PersistentCtx);
        UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[DataAbility] AdvanceGraph RootTask->Execute returned %s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
        if (Status != EAuraAbilityActionStatus::Running)
        {
            UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] AdvanceGraph ending ability because status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));

            // The client intentionally skips authority-only actions such as
            // SpawnProjectiles. If it ends the predicted activation here, GAS
            // replicates that local completion to the server before the server
            // reaches its montage event and projectile node. Keep the client
            // instance alive until the authoritative instance ends it.
            const bool bIsNetAuthority = CurrentActorInfo->IsNetAuthority();
            if (Status == EAuraAbilityActionStatus::Success && !ShouldEndAfterGraphCompletion(bIsNetAuthority))
            {
                bGraphActive = false;
                bWaitingForAuthoritativeEnd = true;
                RootTask = nullptr;
                UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] Graph completed on non-authority client; waiting for authoritative server end"));
                return;
            }

            const FGameplayAbilityActivationInfo& ActivationInfo = GetCurrentActivationInfo();
            EndAbility(CurrentSpecHandle, CurrentActorInfo, ActivationInfo, true, Status == EAuraAbilityActionStatus::Failure);
        }
    }
    else
    {
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[DataAbility] AdvanceGraph failed: RootTask is null"));
    }
}

void UAuraDataAbility::OnTargetDataReady(const FGameplayAbilityTargetDataHandle& DataHandle)
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[DataAbility] OnTargetDataReady DataNum=%d bGraphActive=%s"), DataHandle.Num(), bGraphActive ? TEXT("true") : TEXT("false"));
    if (!bGraphActive)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] OnTargetDataReady ignored: graph not active"));
        return;
    }

    if (!CurrentActorInfo || !CurrentActorInfo->AbilitySystemComponent.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[DataAbility] OnTargetDataReady aborted: stale ActorInfo"));
        return;
    }

    PersistentCtx.ASC = CurrentActorInfo->AbilitySystemComponent.Get();
    PersistentCtx.AvatarActor = CurrentActorInfo->AvatarActor.Get();
    PersistentCtx.TargetDataHandle = DataHandle;
    PersistentCtx.Definition = GetDefinition();

    if (DataHandle.Num() > 0)
    {
        const FGameplayAbilityTargetData* Data = DataHandle.Get(0);
        if (Data && Data->GetHitResult())
        {
            PersistentCtx.CursorHit = *Data->GetHitResult();
            if (PersistentCtx.CursorHit.GetActor())
            {
                UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] OnTargetDataReady hit=%s loc=%s"), *PersistentCtx.CursorHit.GetActor()->GetName(), *PersistentCtx.CursorHit.ImpactPoint.ToString());
            }
        }
    }

    if (PendingTargetDataTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] OnTargetDataReady ending pending target data task"));
        PendingTargetDataTask->EndTask();
        PendingTargetDataTask.Reset();
    }

    AdvanceGraph(EAuraAbilityActionStatus::Success);
}

void UAuraDataAbility::OnMontageEventReceived(FGameplayEventData EventData)
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[DataAbility] OnMontageEventReceived bGraphActive=%s"), bGraphActive ? TEXT("true") : TEXT("false"));
    if (!bGraphActive)
    {
        return;
    }

    if (PendingMontageEventTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] OnMontageEventReceived resetting PendingMontageEventTask"));
        PendingMontageEventTask.Reset();
    }

    AdvanceGraph(EAuraAbilityActionStatus::Success);
}

void UAuraDataAbility::OnMontageCompleted()
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[DataAbility] OnMontageCompleted bGraphActive=%s"), bGraphActive ? TEXT("true") : TEXT("false"));
    if (!bGraphActive)
    {
        return;
    }

    if (PendingMontageTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] OnMontageCompleted resetting PendingMontageTask"));
        PendingMontageTask.Reset();
    }

    AdvanceGraph(EAuraAbilityActionStatus::Success);
}

void UAuraDataAbility::OnMontageInterrupted()
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[DataAbility] OnMontageInterrupted bGraphActive=%s"), bGraphActive ? TEXT("true") : TEXT("false"));
    if (!bGraphActive)
    {
        return;
    }

    if (PendingMontageTask.IsValid())
    {
        PendingMontageTask.Reset();
    }

    AdvanceGraph(EAuraAbilityActionStatus::Failure);
}
