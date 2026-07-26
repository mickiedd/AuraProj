// Copyright Druid Mechanics

#include "Nodes/Actions/WaitForTargetDataNode.h"
#include "Nodes/AbilityActionTask.h"
#include "DataAbility.h"
#include "AbilitySystem/AbilityTasks/TargetDataUnderMouse.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UWaitForTargetDataNode::CreateTask(UObject* Outer) const
{
    return NewObject<UWaitForTargetDataTask>(Outer);
}

EAuraAbilityActionStatus UWaitForTargetDataTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[WaitForTargetData] OnStart OwnerAbility=%s ASC=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.ASC));
    if (!OwnerAbility || !Ctx.ASC)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForTargetData] OnStart abort: missing OwnerAbility or ASC"));
        return EAuraAbilityActionStatus::Failure;
    }

    UTargetDataUnderMouse* Task = UTargetDataUnderMouse::CreateTargetDataUnderMouse(OwnerAbility);
    if (!Task)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[WaitForTargetData] OnStart abort: failed to create TargetDataUnderMouse task"));
        return EAuraAbilityActionStatus::Failure;
    }

    FScriptDelegate Delegate;
    Delegate.BindUFunction(this, GET_FUNCTION_NAME_CHECKED(UWaitForTargetDataTask, OnValidData));
    Task->ValidData.Add(Delegate);
    Task->ReadyForActivation();

    TargetDataTask = Task;
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->PendingTargetDataTask = Task;
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[WaitForTargetData] OnStart waiting for cursor target data"));
    return EAuraAbilityActionStatus::Running;
}

void UWaitForTargetDataTask::OnValidData(const FGameplayAbilityTargetDataHandle& DataHandle)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[WaitForTargetData] OnValidData received DataNum=%d"), DataHandle.Num());
    PendingStatus = EAuraAbilityActionStatus::Success;
    if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        DataAbility->OnTargetDataReady(DataHandle);
    }
}

void UWaitForTargetDataTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[WaitForTargetData] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
    if (TargetDataTask.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[WaitForTargetData] OnExit ending target data task"));
        TargetDataTask->EndTask();
        TargetDataTask.Reset();
    }
}
