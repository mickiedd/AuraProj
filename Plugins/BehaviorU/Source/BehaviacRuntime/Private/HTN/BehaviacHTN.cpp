// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "HTN/BehaviacHTN.h"
#include "BehaviacAgent.h"

// ===================================================================
// HTN TASK
// ===================================================================

UBehaviacBehaviorTask* UBehaviacHTNTask::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacHTNTaskExecution>(Outer);
}

void UBehaviacHTNTask::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bIsPrimitive = true;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("IsPrimitive"))
		{
			bIsPrimitive = (Prop.Value == TEXT("true"));
		}
		else if (Prop.Name == TEXT("ReferencedBehavior"))
		{
			ReferencedTreePath = Prop.Value;
		}
	}
}

EBehaviacStatus UBehaviacHTNTaskExecution::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}
	return EBehaviacStatus::Success;
}

// ===================================================================
// HTN METHOD
// ===================================================================

UBehaviacBehaviorTask* UBehaviacHTNMethod::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacHTNMethodTask>(Outer);
}

void UBehaviacHTNMethod::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Precondition"))
		{
			MethodPrecondition = Prop.Value;
		}
	}
}

EBehaviacStatus UBehaviacHTNMethodTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	// Execute children sequentially (method body)
	while (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviacStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, ChildStatus);

		if (Result == EBehaviacStatus::Running)
		{
			return EBehaviacStatus::Running;
		}

		if (Result == EBehaviacStatus::Failure)
		{
			return EBehaviacStatus::Failure;
		}

		ActiveChildIndex++;
	}

	return EBehaviacStatus::Success;
}

// ===================================================================
// HTN PLANNER
// ===================================================================

UBehaviacHTNPlanner::UBehaviacHTNPlanner()
	: bAutoReplan(true)
	, Agent(nullptr)
	, RootTaskNode(nullptr)
	, CurrentPlanStep(0)
	, CurrentTaskExecution(nullptr)
{
}

void UBehaviacHTNPlanner::Init(UBehaviacAgentComponent* InAgent, UBehaviacHTNTask* InRootTask)
{
	Agent = InAgent;
	RootTaskNode = InRootTask;
	CurrentPlan.Empty();
	CurrentPlanStep = 0;
	CurrentTaskExecution = nullptr;
}

void UBehaviacHTNPlanner::Uninit()
{
	Agent = nullptr;
	RootTaskNode = nullptr;
	CurrentPlan.Empty();
	CurrentPlanStep = 0;
	CurrentTaskExecution = nullptr;
}

EBehaviacStatus UBehaviacHTNPlanner::Update()
{
	if (!Agent || !RootTaskNode)
	{
		return EBehaviacStatus::Failure;
	}

	// Generate plan if we don't have one
	if (CurrentPlan.Num() == 0 || CurrentPlanStep >= CurrentPlan.Num())
	{
		if (!GeneratePlan())
		{
			return EBehaviacStatus::Failure;
		}
		CurrentPlanStep = 0;
	}

	// Execute current plan step
	EBehaviacStatus Result = ExecutePlan();

	// Handle plan failure
	if (Result == EBehaviacStatus::Failure && bAutoReplan)
	{
		CurrentPlan.Empty();
		CurrentPlanStep = 0;
		CurrentTaskExecution = nullptr;
		return EBehaviacStatus::Running; // Will replan next tick
	}

	return Result;
}

bool UBehaviacHTNPlanner::GeneratePlan()
{
	CurrentPlan.Empty();
	return DecomposeTask(RootTaskNode, CurrentPlan, 0);
}

bool UBehaviacHTNPlanner::CanInterruptCurrentPlan() const
{
	return bAutoReplan;
}

EBehaviacStatus UBehaviacHTNPlanner::ExecutePlan()
{
	if (!CurrentPlan.IsValidIndex(CurrentPlanStep))
	{
		return EBehaviacStatus::Success; // Plan completed
	}

	UBehaviacHTNTask* CurrentTask = CurrentPlan[CurrentPlanStep];
	if (!CurrentTask)
	{
		return EBehaviacStatus::Failure;
	}

	// Create execution task if needed
	if (!CurrentTaskExecution)
	{
		CurrentTaskExecution = CurrentTask->CreateTask(this);
		if (CurrentTaskExecution)
		{
			CurrentTaskExecution->Init(CurrentTask);
		}
	}

	if (!CurrentTaskExecution)
	{
		return EBehaviacStatus::Failure;
	}

	EBehaviacStatus Result = CurrentTaskExecution->Execute(Agent, EBehaviacStatus::Running);

	if (Result == EBehaviacStatus::Success)
	{
		// Move to next step
		CurrentPlanStep++;
		CurrentTaskExecution = nullptr;

		if (CurrentPlanStep >= CurrentPlan.Num())
		{
			return EBehaviacStatus::Success; // Plan fully executed
		}

		return EBehaviacStatus::Running;
	}

	return Result;
}

bool UBehaviacHTNPlanner::DecomposeTask(UBehaviacHTNTask* Task, TArray<UBehaviacHTNTask*>& OutPlan, int32 Depth)
{
	if (!Task || Depth >= MAX_DECOMPOSITION_DEPTH)
	{
		return false;
	}

	// Primitive tasks go directly into the plan
	if (Task->bIsPrimitive)
	{
		OutPlan.Add(Task);
		return true;
	}

	// Compound task: try each method
	for (UBehaviacBehaviorNode* ChildNode : Task->Children)
	{
		UBehaviacHTNMethod* Method = Cast<UBehaviacHTNMethod>(ChildNode);
		if (!Method)
		{
			continue;
		}

		// Check method precondition
		if (!Method->MethodPrecondition.IsEmpty() && Agent)
		{
			FString PrecondVal = Agent->GetPropertyValue(Method->MethodPrecondition);
			if (PrecondVal == TEXT("false") || PrecondVal == TEXT("0"))
			{
				continue; // Precondition not met
			}
		}

		// Try to decompose method's subtasks
		TArray<UBehaviacHTNTask*> MethodPlan;
		bool bMethodSuccess = true;

		for (UBehaviacBehaviorNode* SubNode : Method->Children)
		{
			UBehaviacHTNTask* SubTask = Cast<UBehaviacHTNTask>(SubNode);
			if (SubTask)
			{
				if (!DecomposeTask(SubTask, MethodPlan, Depth + 1))
				{
					bMethodSuccess = false;
					break;
				}
			}
		}

		if (bMethodSuccess)
		{
			OutPlan.Append(MethodPlan);
			return true;
		}
	}

	return false; // No method worked
}
