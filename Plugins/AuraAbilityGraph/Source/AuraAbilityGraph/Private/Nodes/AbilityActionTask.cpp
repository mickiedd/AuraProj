// Copyright Druid Mechanics

#include "Nodes/AbilityActionTask.h"
#include "Nodes/AbilityActionNode.h"

void UAuraAbilityActionTask::Init(UAuraAbilityActionNode* InNode, UAuraDataAbility* InOwner)
{
    NodeDef = InNode;
    OwnerAbility = InOwner;
    HasEntered = false;
    PendingStatus = EAuraAbilityActionStatus::Invalid;

    if (InNode)
    {
        ChildTasks.Empty();
        ChildTasks.Reserve(InNode->Children.Num());
        for (UAuraAbilityActionNode* ChildNode : InNode->Children)
        {
            if (ChildNode)
            {
                UAuraAbilityActionTask* ChildTask = ChildNode->CreateTask(this);
                if (ChildTask)
                {
                    ChildTask->Init(ChildNode, InOwner);
                    ChildTask->ParentTask = this;
                    ChildTasks.Add(ChildTask);
                }
            }
        }
    }
}

EAuraAbilityActionStatus UAuraAbilityActionTask::Execute(FAuraAbilityExecutionContext& Ctx)
{
    if (!NodeDef)
    {
        return EAuraAbilityActionStatus::Failure;
    }

    if (HasEntered && PendingStatus != EAuraAbilityActionStatus::Invalid)
    {
        EAuraAbilityActionStatus Result = PendingStatus;
        PendingStatus = EAuraAbilityActionStatus::Invalid;
        OnExit(Ctx, Result);
        HasEntered = false;
        return Result;
    }

    if (!HasEntered)
    {
        OnEnter(Ctx);
        HasEntered = true;
    }

    EAuraAbilityActionStatus Result = OnStart(Ctx);

    if (Result != EAuraAbilityActionStatus::Running)
    {
        OnExit(Ctx, Result);
        HasEntered = false;
    }

    return Result;
}
