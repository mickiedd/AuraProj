// Copyright Druid Mechanics

#include "DataAbility.h"
#include "AbilityDefinition.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystem/AuraAttributeSet.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
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
        EndAbility(Handle, ActorInfo, ActivationInfo, true, Status == EAuraAbilityActionStatus::Failure);
        return;
    }
}

void UAuraDataAbility::EndAbility(const FGameplayAbilitySpecHandle Handle, const FGameplayAbilityActorInfo* ActorInfo, const FGameplayAbilityActivationInfo ActivationInfo, bool bReplicateEndAbility, bool bWasCancelled)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[DataAbility] EndAbility START handle=%s bWasCancelled=%s"), *Handle.ToString(), bWasCancelled ? TEXT("true") : TEXT("false"));
    FAuraAbilityExecutionContext Ctx;
    Ctx.ASC = ActorInfo->AbilitySystemComponent.Get();
    Ctx.AvatarActor = ActorInfo->AvatarActor.Get();
    Ctx.SpecHandle = Handle;

    if (RootTask)
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] EndAbility cancelling RootTask"));
        FAuraAbilityExecutionContext CancelCtx;
        CancelCtx.ASC = ActorInfo->AbilitySystemComponent.Get();
        CancelCtx.AvatarActor = ActorInfo->AvatarActor.Get();
        CancelCtx.SpecHandle = Handle;

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

    bGraphActive = false;
    PersistentCtx.TargetDataHandle.Clear();
    PersistentCtx.CursorHit = FHitResult();
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] EndAbility END"));
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
        UAbilitySystemBlueprintLibrary::AssignTagSetByCallerMagnitude(SpecHandle, FGameplayTag::RequestGameplayTag(FName("Abilities.Cost.Mana"), false), -GetDefinition()->ManaCost);
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

    if (RootTask)
    {
        EAuraAbilityActionStatus Status = RootTask->Execute(PersistentCtx);
        UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[DataAbility] AdvanceGraph RootTask->Execute returned %s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
        if (Status != EAuraAbilityActionStatus::Running)
        {
            UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[DataAbility] AdvanceGraph ending ability because status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
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
