// Copyright Druid Mechanics

#include "Nodes/Composites/SequenceNode.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* UAuraSequenceNode::CreateTask(UObject* Outer) const
{
    return NewObject<UAuraSequenceTask>(Outer);
}

EAuraAbilityActionStatus UAuraSequenceTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[Sequence] OnStart ActiveChildIndex=%d NumChildren=%d"), ActiveChildIndex, ChildTasks.Num());

    while (ActiveChildIndex < ChildTasks.Num())
    {
        UAuraAbilityActionTask* CurrentChild = ChildTasks[ActiveChildIndex];
        if (!CurrentChild)
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Sequence] OnStart skipping null child at index %d"), ActiveChildIndex);
            ++ActiveChildIndex;
            continue;
        }

        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[Sequence] OnStart executing child[%d] class=%s"), ActiveChildIndex, *CurrentChild->GetClass()->GetName());
        EAuraAbilityActionStatus ChildResult = CurrentChild->Execute(Ctx);
        UE_LOG(LogAuraAbilityGraph, VeryVerbose, TEXT("[Sequence] OnStart child[%d] returned %s"), ActiveChildIndex, *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(ChildResult));

        if (ChildResult == EAuraAbilityActionStatus::Success)
        {
            ++ActiveChildIndex;
            continue;
        }
        else if (ChildResult == EAuraAbilityActionStatus::Failure)
        {
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Sequence] OnStart child[%d] failed, sequence failing"), ActiveChildIndex);
            return EAuraAbilityActionStatus::Failure;
        }

        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[Sequence] OnStart child[%d] running, sequence waiting"), ActiveChildIndex);
        return EAuraAbilityActionStatus::Running;
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[Sequence] OnStart sequence complete"));
    return EAuraAbilityActionStatus::Success;
}

void UAuraSequenceTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[Sequence] OnExit status=%s cancelling %d children"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status), ChildTasks.Num());
    for (UAuraAbilityActionTask* Child : ChildTasks)
    {
        if (Child && Child->HasEntered)
        {
            Child->Cancel(Ctx);
            Child->HasEntered = false;
        }
    }
    // Reset state for potential reuse; tasks are recreated per activation so a fresh index is correct regardless of Success/Cancel.
    ActiveChildIndex = 0;
}
