// Copyright Druid Mechanics

#include "Nodes/Actions/PlayMontageNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_PlayMontageAndWait.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UPlayMontageNode::CreateTask(UObject* Outer) const
{
    return NewObject<UPlayMontageTask>(Outer);
}

EAuraAbilityActionStatus UPlayMontageTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnStart OwnerAbility=%s"), *GetNameSafe(OwnerAbility));
    if (!OwnerAbility)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[PlayMontage] OnStart abort: OwnerAbility null"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UAuraAbilityDefinition* Definition = Ctx.Definition;
    if (!Definition || !Definition->Montage)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[PlayMontage] OnStart abort: no montage on definition"));
        return EAuraAbilityActionStatus::Success;
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnStart playing montage=%s"), *Definition->Montage.GetName());
    UAbilityTask_PlayMontageAndWait* Task = UAbilityTask_PlayMontageAndWait::CreatePlayMontageAndWaitProxy(
        OwnerAbility,
        FName("PlayMontage"),
        Definition->Montage,
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
        // Wire the montage task's interruption delegates to the ability. Without these,
        // an interrupted/blended-out montage would never signal the graph, leaving it
        // hung in a Running state forever (EndAbility never fires). OnMontageInterrupted
        // advances the graph with Failure, ending the ability as cancelled. The graph's
        // bGraphActive guard makes late callbacks (after a normal event-driven completion)
        // a safe no-op.
        Task->OnInterrupted.AddDynamic(DataAbility, &UAuraDataAbility::OnMontageInterrupted);
        Task->OnBlendOut.AddDynamic(DataAbility, &UAuraDataAbility::OnMontageInterrupted);
    }

    Task->ReadyForActivation();

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnStart task activated"));
    return EAuraAbilityActionStatus::Success;
}

void UPlayMontageTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[PlayMontage] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
}
