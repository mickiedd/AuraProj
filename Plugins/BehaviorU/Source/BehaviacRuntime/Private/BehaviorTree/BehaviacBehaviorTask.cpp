// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/BehaviacBehaviorTask.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/Attachments/BehaviacAttachment.h"
#include "BehaviacAgent.h"
#include "BehaviacStats.h"

// ===================================================================
// UBehaviacBehaviorTask
// ===================================================================

UBehaviacBehaviorTask::UBehaviacBehaviorTask()
	: Node(nullptr)
	, Status(EBehaviacStatus::Invalid)
	, ParentTask(nullptr)
	, bHasEntered(false)
{
}

void UBehaviacBehaviorTask::Init(UBehaviacBehaviorNode* InNode)
{
	Node = InNode;
	Status = EBehaviacStatus::Invalid;
}

EBehaviacStatus UBehaviacBehaviorTask::Execute(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	SCOPE_CYCLE_COUNTER(STAT_Behaviac_Task_Execute);

	if (!Node || !Node->IsValid(Agent, this))
	{
		return EBehaviacStatus::Failure;
	}

	EBehaviacStatus Result = EBehaviacStatus::Running;

	// Enter phase
	if (!bHasEntered)
	{
		// Check enter preconditions
		if (!CheckPreconditions(Agent, false))
		{
			return EBehaviacStatus::Failure;
		}

		bHasEntered = true;

		if (!OnEnter(Agent))
		{
			bHasEntered = false;
			return EBehaviacStatus::Failure;
		}
	}

	// Check update preconditions
	if (!CheckPreconditions(Agent, true))
	{
		Result = EBehaviacStatus::Failure;
	}
	else
	{
		Result = UpdateCurrent(Agent, ChildStatus);
	}

	// Exit phase
	if (Result != EBehaviacStatus::Running)
	{
		ApplyEffectors(Agent, Result);
		OnExit(Agent, Result);
		bHasEntered = false;
		Status = Result;
	}
	else
	{
		Status = EBehaviacStatus::Running;
	}

	return Result;
}

void UBehaviacBehaviorTask::Reset(UBehaviacAgentComponent* Agent)
{
	Status = EBehaviacStatus::Invalid;
	bHasEntered = false;
}

void UBehaviacBehaviorTask::Traverse(bool bChildFirst, TFunction<bool(UBehaviacBehaviorTask*)> Handler)
{
	Handler(this);
}

bool UBehaviacBehaviorTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	return true;
}

void UBehaviacBehaviorTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
}

EBehaviacStatus UBehaviacBehaviorTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	return EBehaviacStatus::Success;
}

EBehaviacStatus UBehaviacBehaviorTask::UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	return OnUpdate(Agent, ChildStatus);
}

bool UBehaviacBehaviorTask::CheckPreconditions(UBehaviacAgentComponent* Agent, bool bIsUpdate) const
{
	SCOPE_CYCLE_COUNTER(STAT_Behaviac_Task_Precond);

	if (!Node)
	{
		return true;
	}

	for (UBehaviacAttachment* Precondition : Node->Preconditions)
	{
		if (!Precondition)
		{
			continue;
		}

		// Check if this precondition applies to the current phase
		if (!Precondition->AppliesToPhase(bIsUpdate ? EBehaviacPreconditionPhase::Update : EBehaviacPreconditionPhase::Enter))
		{
			continue;
		}

		if (!Precondition->Evaluate(Agent))
		{
			return false;
		}
	}

	return true;
}

void UBehaviacBehaviorTask::ApplyEffectors(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) const
{
	SCOPE_CYCLE_COUNTER(STAT_Behaviac_Task_Effectors);

	if (!Node)
	{
		return;
	}

	for (UBehaviacAttachment* Effector : Node->Effectors)
	{
		if (!Effector)
		{
			continue;
		}

		const bool bApplyOnSuccess = (InStatus == EBehaviacStatus::Success);
		Effector->Apply(Agent, bApplyOnSuccess);
	}
}

// ===================================================================
// UBehaviacCompositeTask
// ===================================================================

UBehaviacCompositeTask::UBehaviacCompositeTask()
	: ActiveChildIndex(0)
{
}

void UBehaviacCompositeTask::Init(UBehaviacBehaviorNode* InNode)
{
	Super::Init(InNode);
	ActiveChildIndex = 0;

	if (InNode)
	{
		ChildTasks.Empty();
		ChildTasks.Reserve(InNode->GetChildCount());

		for (int32 i = 0; i < InNode->GetChildCount(); i++)
		{
			UBehaviacBehaviorNode* ChildNode = InNode->GetChild(i);
			if (ChildNode)
			{
				UBehaviacBehaviorTask* ChildTask = ChildNode->CreateTask(this);
				if (ChildTask)
				{
					ChildTask->Init(ChildNode);
					ChildTask->SetParentTask(this);
					ChildTasks.Add(ChildTask);
				}
			}
		}
	}
}

void UBehaviacCompositeTask::Reset(UBehaviacAgentComponent* Agent)
{
	Super::Reset(Agent);
	ActiveChildIndex = 0;

	for (UBehaviacBehaviorTask* ChildTask : ChildTasks)
	{
		if (ChildTask)
		{
			ChildTask->Reset(Agent);
		}
	}
}

void UBehaviacCompositeTask::Traverse(bool bChildFirst, TFunction<bool(UBehaviacBehaviorTask*)> Handler)
{
	if (!bChildFirst)
	{
		if (!Handler(this))
		{
			return;
		}
	}

	for (UBehaviacBehaviorTask* ChildTask : ChildTasks)
	{
		if (ChildTask)
		{
			ChildTask->Traverse(bChildFirst, Handler);
		}
	}

	if (bChildFirst)
	{
		Handler(this);
	}
}

// ===================================================================
// UBehaviacSingleChildTask
// ===================================================================

UBehaviacSingleChildTask::UBehaviacSingleChildTask()
	: ChildTask(nullptr)
{
}

void UBehaviacSingleChildTask::Init(UBehaviacBehaviorNode* InNode)
{
	Super::Init(InNode);

	if (InNode && InNode->GetChildCount() > 0)
	{
		UBehaviacBehaviorNode* ChildNode = InNode->GetChild(0);
		if (ChildNode)
		{
			ChildTask = ChildNode->CreateTask(this);
			if (ChildTask)
			{
				ChildTask->Init(ChildNode);
				ChildTask->SetParentTask(this);
			}
		}
	}
}

void UBehaviacSingleChildTask::Reset(UBehaviacAgentComponent* Agent)
{
	Super::Reset(Agent);
	if (ChildTask)
	{
		ChildTask->Reset(Agent);
	}
}

void UBehaviacSingleChildTask::Traverse(bool bChildFirst, TFunction<bool(UBehaviacBehaviorTask*)> Handler)
{
	if (!bChildFirst)
	{
		if (!Handler(this))
		{
			return;
		}
	}

	if (ChildTask)
	{
		ChildTask->Traverse(bChildFirst, Handler);
	}

	if (bChildFirst)
	{
		Handler(this);
	}
}

EBehaviacStatus UBehaviacSingleChildTask::UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildTask)
	{
		EBehaviacStatus Result = ChildTask->Execute(Agent, ChildStatus);
		return Result;
	}

	return EBehaviacStatus::Failure;
}

// ===================================================================
// UBehaviacBehaviorTreeTask
// ===================================================================

void UBehaviacBehaviorTreeTask::Init(UBehaviacBehaviorNode* InNode)
{
	// Don't call Super::Init() because SingleChildTask expects InNode to HAVE children
	// Instead, create a task directly FROM the root node
	
	Node = InNode;  // Set our node reference
	Status = EBehaviacStatus::Invalid;
	
	if (InNode)
	{
		// Create task from the root node itself (not from its child!)
		ChildTask = InNode->CreateTask(this);
		if (ChildTask)
		{
			ChildTask->Init(InNode);
			ChildTask->SetParentTask(this);
			
			BEHAVIAC_VLOG(TEXT("[Behaviac] BehaviorTreeTask created ChildTask from node '%s'"), 
				*InNode->GetName());
		}
		else
		{
			UE_LOG(LogBehaviac, Error, TEXT("[Behaviac] Node->CreateTask() returned NULL for node '%s'"), 
				*InNode->GetName());
		}
	}
}

EBehaviacStatus UBehaviacBehaviorTreeTask::Tick(UBehaviacAgentComponent* Agent)
{
	return Execute(Agent, EBehaviacStatus::Invalid);
}

bool UBehaviacBehaviorTreeTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	return true;
}

void UBehaviacBehaviorTreeTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
	Super::OnExit(Agent, InStatus);
}

EBehaviacStatus UBehaviacBehaviorTreeTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}

	return EBehaviacStatus::Failure;
}
