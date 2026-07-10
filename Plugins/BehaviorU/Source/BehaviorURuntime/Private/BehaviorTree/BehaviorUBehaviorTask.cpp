// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/Attachments/BehaviorUAttachment.h"
#include "BehaviorUAgent.h"
#include "BehaviorUStats.h"

// ===================================================================
// UBehaviorUBehaviorTask
// ===================================================================

UBehaviorUBehaviorTask::UBehaviorUBehaviorTask()
	: Node(nullptr)
	, Status(EBehaviorUStatus::Invalid)
	, ParentTask(nullptr)
	, bHasEntered(false)
{
}

void UBehaviorUBehaviorTask::Init(UBehaviorUBehaviorNode* InNode)
{
	Node = InNode;
	Status = EBehaviorUStatus::Invalid;
}

EBehaviorUStatus UBehaviorUBehaviorTask::Execute(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	SCOPE_CYCLE_COUNTER(STAT_BehaviorU_Task_Execute);

	if (!Node || !Node->IsValid(Agent, this))
	{
		return EBehaviorUStatus::Failure;
	}

	EBehaviorUStatus Result = EBehaviorUStatus::Running;

	// Enter phase
	if (!bHasEntered)
	{
		// Check enter preconditions
		if (!CheckPreconditions(Agent, false))
		{
			return EBehaviorUStatus::Failure;
		}

		bHasEntered = true;

		if (!OnEnter(Agent))
		{
			bHasEntered = false;
			return EBehaviorUStatus::Failure;
		}
	}

	// Check update preconditions
	if (!CheckPreconditions(Agent, true))
	{
		Result = EBehaviorUStatus::Failure;
	}
	else
	{
		Result = UpdateCurrent(Agent, ChildStatus);
	}

	// Exit phase
	if (Result != EBehaviorUStatus::Running)
	{
		ApplyEffectors(Agent, Result);
		OnExit(Agent, Result);
		bHasEntered = false;
		Status = Result;
	}
	else
	{
		Status = EBehaviorUStatus::Running;
	}

	return Result;
}

void UBehaviorUBehaviorTask::Reset(UBehaviorUAgentComponent* Agent)
{
	Status = EBehaviorUStatus::Invalid;
	bHasEntered = false;
}

void UBehaviorUBehaviorTask::Traverse(bool bChildFirst, TFunction<bool(UBehaviorUBehaviorTask*)> Handler)
{
	Handler(this);
}

bool UBehaviorUBehaviorTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	return true;
}

void UBehaviorUBehaviorTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
}

EBehaviorUStatus UBehaviorUBehaviorTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	return EBehaviorUStatus::Success;
}

EBehaviorUStatus UBehaviorUBehaviorTask::UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	return OnUpdate(Agent, ChildStatus);
}

bool UBehaviorUBehaviorTask::CheckPreconditions(UBehaviorUAgentComponent* Agent, bool bIsUpdate) const
{
	SCOPE_CYCLE_COUNTER(STAT_BehaviorU_Task_Precond);

	if (!Node)
	{
		return true;
	}

	for (UBehaviorUAttachment* Precondition : Node->Preconditions)
	{
		if (!Precondition)
		{
			continue;
		}

		// Check if this precondition applies to the current phase
		if (!Precondition->AppliesToPhase(bIsUpdate ? EBehaviorUPreconditionPhase::Update : EBehaviorUPreconditionPhase::Enter))
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

void UBehaviorUBehaviorTask::ApplyEffectors(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) const
{
	SCOPE_CYCLE_COUNTER(STAT_BehaviorU_Task_Effectors);

	if (!Node)
	{
		return;
	}

	for (UBehaviorUAttachment* Effector : Node->Effectors)
	{
		if (!Effector)
		{
			continue;
		}

		const bool bApplyOnSuccess = (InStatus == EBehaviorUStatus::Success);
		Effector->Apply(Agent, bApplyOnSuccess);
	}
}

// ===================================================================
// UBehaviorUCompositeTask
// ===================================================================

UBehaviorUCompositeTask::UBehaviorUCompositeTask()
	: ActiveChildIndex(0)
{
}

void UBehaviorUCompositeTask::Init(UBehaviorUBehaviorNode* InNode)
{
	Super::Init(InNode);
	ActiveChildIndex = 0;

	if (InNode)
	{
		ChildTasks.Empty();
		ChildTasks.Reserve(InNode->GetChildCount());

		for (int32 i = 0; i < InNode->GetChildCount(); i++)
		{
			UBehaviorUBehaviorNode* ChildNode = InNode->GetChild(i);
			if (ChildNode)
			{
				UBehaviorUBehaviorTask* ChildTask = ChildNode->CreateTask(this);
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

void UBehaviorUCompositeTask::Reset(UBehaviorUAgentComponent* Agent)
{
	Super::Reset(Agent);
	ActiveChildIndex = 0;

	for (UBehaviorUBehaviorTask* ChildTask : ChildTasks)
	{
		if (ChildTask)
		{
			ChildTask->Reset(Agent);
		}
	}
}

void UBehaviorUCompositeTask::Traverse(bool bChildFirst, TFunction<bool(UBehaviorUBehaviorTask*)> Handler)
{
	if (!bChildFirst)
	{
		if (!Handler(this))
		{
			return;
		}
	}

	for (UBehaviorUBehaviorTask* ChildTask : ChildTasks)
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
// UBehaviorUSingleChildTask
// ===================================================================

UBehaviorUSingleChildTask::UBehaviorUSingleChildTask()
	: ChildTask(nullptr)
{
}

void UBehaviorUSingleChildTask::Init(UBehaviorUBehaviorNode* InNode)
{
	Super::Init(InNode);

	if (InNode && InNode->GetChildCount() > 0)
	{
		UBehaviorUBehaviorNode* ChildNode = InNode->GetChild(0);
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

void UBehaviorUSingleChildTask::Reset(UBehaviorUAgentComponent* Agent)
{
	Super::Reset(Agent);
	if (ChildTask)
	{
		ChildTask->Reset(Agent);
	}
}

void UBehaviorUSingleChildTask::Traverse(bool bChildFirst, TFunction<bool(UBehaviorUBehaviorTask*)> Handler)
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

EBehaviorUStatus UBehaviorUSingleChildTask::UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildTask)
	{
		EBehaviorUStatus Result = ChildTask->Execute(Agent, ChildStatus);
		return Result;
	}

	return EBehaviorUStatus::Failure;
}

// ===================================================================
// UBehaviorUBehaviorTreeTask
// ===================================================================

void UBehaviorUBehaviorTreeTask::Init(UBehaviorUBehaviorNode* InNode)
{
	// Don't call Super::Init() because SingleChildTask expects InNode to HAVE children
	// Instead, create a task directly FROM the root node
	
	Node = InNode;  // Set our node reference
	Status = EBehaviorUStatus::Invalid;
	
	if (InNode)
	{
		// Create task from the root node itself (not from its child!)
		ChildTask = InNode->CreateTask(this);
		if (ChildTask)
		{
			ChildTask->Init(InNode);
			ChildTask->SetParentTask(this);
			
			BEHAVIORU_VLOG(TEXT("[BehaviorU] BehaviorTreeTask created ChildTask from node '%s'"), 
				*InNode->GetName());
		}
		else
		{
			UE_LOG(LogBehaviorU, Error, TEXT("[BehaviorU] Node->CreateTask() returned NULL for node '%s'"), 
				*InNode->GetName());
		}
	}
}

EBehaviorUStatus UBehaviorUBehaviorTreeTask::Tick(UBehaviorUAgentComponent* Agent)
{
	return Execute(Agent, EBehaviorUStatus::Invalid);
}

bool UBehaviorUBehaviorTreeTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	return true;
}

void UBehaviorUBehaviorTreeTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
	Super::OnExit(Agent, InStatus);
}

EBehaviorUStatus UBehaviorUBehaviorTreeTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}

	return EBehaviorUStatus::Failure;
}
