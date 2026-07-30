// Copyright Druid Mechanics

#include "Nodes/Actions/WaitNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "DataAbility.h"
#include "AbilityGraphTypes.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Engine/World.h"
#include "TimerManager.h"

UAuraAbilityActionTask* UWaitNode::CreateTask(UObject* Outer) const
{
    return NewObject<UWaitTask>(Outer);
}

void UWaitNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("Seconds"))
        {
            Seconds = FCString::Atof(*Property.Value);
        }
    }
}

EAuraAbilityActionStatus UWaitTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[Wait] OnStart OwnerAbility=%s"), *GetNameSafe(OwnerAbility));
    if (!OwnerAbility)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Wait] OnStart abort: OwnerAbility null"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UWaitNode* Node = Cast<UWaitNode>(NodeDef);
    const float WaitSeconds = Node ? Node->Seconds : 0.5f;

    UWorld* World = OwnerAbility->GetWorld();
    if (!World)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Wait] OnStart abort: no World"));
        return EAuraAbilityActionStatus::Failure;
    }

    FTimerDelegate TimerDel;
    // Weak capture — see ElectrocuteBeamNode.cpp: a strong capture would root this task (and its
    // Outer chain up to the World) via the timer delegate, leaking the PIE World on teardown
    // while the wait is still pending. BindWeakLambda already gates on OwnerAbility.
    TimerDel.BindWeakLambda(OwnerAbility, [ThisObj = TWeakObjectPtr<UWaitTask>(this)]()
    {
        UWaitTask* Task = ThisObj.Get();
        if (!Task || !Task->OwnerAbility)
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Wait] Timer fired but task/OwnerAbility is null"));
            return;
        }

        Task->PendingStatus = EAuraAbilityActionStatus::Success;

        if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(Task->OwnerAbility))
        {
            DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
        }
    });

    World->GetTimerManager().SetTimer(TimerHandle, TimerDel, WaitSeconds, false);

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[Wait] OnStart waiting %.2f seconds"), WaitSeconds);
    return EAuraAbilityActionStatus::Running;
}

void UWaitTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[Wait] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));

    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(TimerHandle);
    }
}