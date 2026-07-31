// Copyright Druid Mechanics

#include "Nodes/Actions/PlayMontageNode.h"
#include "Nodes/AbilityActionTask.h"
#include "DataAbility.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UPlayMontageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UPlayMontageTask>(Outer);
}

void UPlayMontageNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("Montage"))
        {
            MontagePath = Property.Value;
            if (!MontagePath.IsEmpty())
            {
                Montage = LoadObject<UAnimMontage>(nullptr, *MontagePath);
                if (!Montage)
                {
                    UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[PlayMontage] LoadFromProperties: failed to load montage '%s'"), *MontagePath);
                }
            }
        }
    }
}

EAuraAbilityActionStatus UPlayMontageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnStart OwnerAbility=%s"), *GetNameSafe(OwnerAbility));
    if (!OwnerAbility)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[PlayMontage] OnStart abort: OwnerAbility null"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UPlayMontageNode* PlayNode = Cast<UPlayMontageNode>(NodeDef);
    UAnimMontage* Montage = PlayNode ? PlayNode->Montage.Get() : nullptr;
    if (!Montage)
    {
        // No montage configured, or the path failed to load. A missing/empty path is
        // treated as "intentionally no animation" -> skip and continue the graph. A set
        // path that failed to load is a broken config -> fail fast so it surfaces.
        if (PlayNode && !PlayNode->MontagePath.IsEmpty())
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[PlayMontage] OnStart abort: montage '%s' failed to load"), *PlayNode->MontagePath);
            return EAuraAbilityActionStatus::Failure;
        }
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[PlayMontage] OnStart: no montage on node, skipping"));
        return EAuraAbilityActionStatus::Success;
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnStart playing montage=%s"), *Montage->GetName());
    UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        OwnerAbility,
        FName("PlayMontage"),
        Montage,
        1.0f,
        NAME_None,
        true,
        1.0f,
        0.0f,
        false);

    if (!Task)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[PlayMontage] OnStart abort: CreatePlayMontageAndWaitProxy returned null"));
        return EAuraAbilityActionStatus::Failure;
    }

    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->PendingMontageTask = Task;
        // Wire only the true-interruption delegate. OnInterrupted fires when the montage
        // is bumped by another montage/ability — that's the case where the graph would
        // otherwise hang in Running forever (no event will ever arrive), so we advance
        // with Failure and AdvanceGraph ends the ability as cancelled. We deliberately
        // do NOT bind OnBlendOut: a montage blends out on natural completion too, and for
        // a channeled ability (e.g. Electrocute) the beam must keep channeling after the
        // short cast montage ends. Binding OnBlendOut would cancel the ability the moment
        // the cast animation finished, cutting the beam short. The graph's bGraphActive
        // guard makes any late callback (after a normal event-driven completion) a no-op.
        Task->OnInterrupted.AddDynamic(DataAbility, &UAuraDataAbility::OnMontageInterrupted);
    }

    Task->ReadyForActivation();

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnStart task activated"));
    return EAuraAbilityActionStatus::Success;
}

void UPlayMontageTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
}
