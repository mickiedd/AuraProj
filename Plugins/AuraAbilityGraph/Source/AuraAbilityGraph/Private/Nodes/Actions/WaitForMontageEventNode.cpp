// Copyright Druid Mechanics

#include "Nodes/Actions/WaitForMontageEventNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/Abilities/AuraGameplayAbility.h"
#include "Abilities/Tasks/AbilityTask_WaitGameplayEvent.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UWaitForMontageEventNode::CreateTask(UObject* Outer) const
{
    return NewObject<UWaitForMontageEventTask>(Outer);
}

void UWaitForMontageEventNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("EventTag"))
        {
            EventTag = FGameplayTag::RequestGameplayTag(FName(*Property.Value), false);
        }
    }
}

EAuraAbilityActionStatus UWaitForMontageEventTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnStart OwnerAbility=%s"), *GetNameSafe(OwnerAbility));
    if (!OwnerAbility)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] OnStart abort: OwnerAbility null"));
        return EAuraAbilityActionStatus::Failure;
    }

    FGameplayTag EventTag = NodeDef ? Cast<UWaitForMontageEventNode>(NodeDef)->EventTag : FGameplayTag();
    if (!EventTag.IsValid())
    {
        if (const UAuraAbilityDefinition* Definition = Ctx.Definition)
        {
            EventTag = Definition->MontageEventTag;
        }
    }
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnStart EventTag=%s"), *EventTag.ToString());

    UAbilityTask_WaitGameplayEvent* Task = UAbilityTask_WaitGameplayEvent::WaitGameplayEvent(OwnerAbility, EventTag, nullptr, false, true);
    if (!Task)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForMontageEvent] OnStart abort: WaitGameplayEvent returned null"));
        return EAuraAbilityActionStatus::Failure;
    }

    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UWaitForMontageEventTask, OnEventReceived));
    Task->EventReceived.Add(Delegate);

    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->PendingMontageEventTask = Task;
    }

    Task->ReadyForActivation();

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnStart waiting for event"));
    return EAuraAbilityActionStatus::Running;
}

void UWaitForMontageEventTask::OnEventReceived(FGameplayEventData EventData)
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[WaitForMontageEvent] OnEventReceived received"));
    PendingStatus = EAuraAbilityActionStatus::Success;
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->PendingMontageEventTask.Reset();
        DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
    }
}

void UWaitForMontageEventTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[WaitForMontageEvent] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
}
