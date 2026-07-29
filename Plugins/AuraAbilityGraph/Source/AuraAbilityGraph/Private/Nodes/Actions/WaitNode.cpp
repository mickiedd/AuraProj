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
    TimerDel.BindWeakLambda(OwnerAbility, [ThisObj = TStrongObjectPtr<UWaitTask>(this)]()
    {
        if (!ThisObj->OwnerAbility)
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Wait] Timer fired but OwnerAbility is null"));
            return;
        }

        ThisObj->PendingStatus = EAuraAbilityActionStatus::Success;

        if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(ThisObj->OwnerAbility))
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