// Copyright Druid Mechanics

#include "Nodes/Composites/SequenceNode.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UAuraSequenceNode::CreateTask(UObject* Outer) const
{
    return NewObject<UAuraSequenceTask>(Outer);
}

EAuraAbilityActionStatus UAuraSequenceTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[Sequence] OnStart ActiveChildIndex=%d NumChildren=%d"), ActiveChildIndex, ChildTasks.Num());
    if (ActiveChildIndex >= ChildTasks.Num())
    {
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[Sequence] OnStart complete: all children finished"));
        return EAuraAbilityActionStatus::Success;
    }

    UAuraAbilityActionTask* CurrentChild = ChildTasks[ActiveChildIndex];
    if (!CurrentChild)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Sequence] OnStart skipping null child at index %d"), ActiveChildIndex);
        ++ActiveChildIndex;
        return OnStart(Ctx);
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[Sequence] OnStart executing child[%d] class=%s"), ActiveChildIndex, *CurrentChild->GetClass()->GetName());
    EAuraAbilityActionStatus ChildResult = CurrentChild->Execute(Ctx);
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[Sequence] OnStart child[%d] returned %s"), ActiveChildIndex, *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(ChildResult));

    if (ChildResult == EAuraAbilityActionStatus::Success)
    {
        ++ActiveChildIndex;
        if (ActiveChildIndex >= ChildTasks.Num())
        {
            UE_LOG(LogAuraAbilityGraph, Log, TEXT("[Sequence] OnStart sequence complete"));
            return EAuraAbilityActionStatus::Success;
        }
        return OnStart(Ctx);
    }
    else if (ChildResult == EAuraAbilityActionStatus::Failure)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Sequence] OnStart child[%d] failed, sequence failing"), ActiveChildIndex);
        return EAuraAbilityActionStatus::Failure;
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[Sequence] OnStart child[%d] running, sequence waiting"), ActiveChildIndex);
    return EAuraAbilityActionStatus::Running;
}

void UAuraSequenceTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[Sequence] OnExit status=%s cancelling %d children"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status), ChildTasks.Num());
    for (UAuraAbilityActionTask* Child : ChildTasks)
    {
        if (Child && Child->HasEntered)
        {
            Child->Cancel(Ctx);
            Child->HasEntered = false;
        }
    }
    ActiveChildIndex = 0;
}
