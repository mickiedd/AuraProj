// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "HTN/BehaviorUHTN.h"
#include "BehaviorUAgent.h"

// ===================================================================
// HTN TASK
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUHTNTask::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUHTNTaskExecution>(Outer);
}

void UBehaviorUHTNTask::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bIsPrimitive = true;

	for (const FBehaviorUProperty& Prop : Properties)
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

EBehaviorUStatus UBehaviorUHTNTaskExecution::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}
	return EBehaviorUStatus::Success;
}

// ===================================================================
// HTN METHOD
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUHTNMethod::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUHTNMethodTask>(Outer);
}

void UBehaviorUHTNMethod::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Precondition"))
		{
			MethodPrecondition = Prop.Value;
		}
	}
}

EBehaviorUStatus UBehaviorUHTNMethodTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	// Execute children sequentially (method body)
	while (ChildTasks.IsValidIndex(ActiveChildIndex))
	{
		EBehaviorUStatus Result = ChildTasks[ActiveChildIndex]->Execute(Agent, ChildStatus);

		if (Result == EBehaviorUStatus::Running)
		{
			return EBehaviorUStatus::Running;
		}

		if (Result == EBehaviorUStatus::Failure)
		{
			return EBehaviorUStatus::Failure;
		}

		ActiveChildIndex++;
	}

	return EBehaviorUStatus::Success;
}

// ===================================================================
// HTN PLANNER
// ===================================================================

UBehaviorUHTNPlanner::UBehaviorUHTNPlanner()
	: bAutoReplan(true)
	, Agent(nullptr)
	, RootTaskNode(nullptr)
	, CurrentPlanStep(0)
	, CurrentTaskExecution(nullptr)
{
}

void UBehaviorUHTNPlanner::Init(UBehaviorUAgentComponent* InAgent, UBehaviorUHTNTask* InRootTask)
{
	Agent = InAgent;
	RootTaskNode = InRootTask;
	CurrentPlan.Empty();
	CurrentPlanStep = 0;
	CurrentTaskExecution = nullptr;
}

void UBehaviorUHTNPlanner::Uninit()
{
	Agent = nullptr;
	RootTaskNode = nullptr;
	CurrentPlan.Empty();
	CurrentPlanStep = 0;
	CurrentTaskExecution = nullptr;
}

EBehaviorUStatus UBehaviorUHTNPlanner::Update()
{
	if (!Agent || !RootTaskNode)
	{
		return EBehaviorUStatus::Failure;
	}

	// Generate plan if we don't have one
	if (CurrentPlan.Num() == 0 || CurrentPlanStep >= CurrentPlan.Num())
	{
		if (!GeneratePlan())
		{
			return EBehaviorUStatus::Failure;
		}
		CurrentPlanStep = 0;
	}

	// Execute current plan step
	EBehaviorUStatus Result = ExecutePlan();

	// Handle plan failure
	if (Result == EBehaviorUStatus::Failure && bAutoReplan)
	{
		CurrentPlan.Empty();
		CurrentPlanStep = 0;
		CurrentTaskExecution = nullptr;
		return EBehaviorUStatus::Running; // Will replan next tick
	}

	return Result;
}

bool UBehaviorUHTNPlanner::GeneratePlan()
{
	CurrentPlan.Empty();
	return DecomposeTask(RootTaskNode, CurrentPlan, 0);
}

bool UBehaviorUHTNPlanner::CanInterruptCurrentPlan() const
{
	return bAutoReplan;
}

EBehaviorUStatus UBehaviorUHTNPlanner::ExecutePlan()
{
	if (!CurrentPlan.IsValidIndex(CurrentPlanStep))
	{
		return EBehaviorUStatus::Success; // Plan completed
	}

	UBehaviorUHTNTask* CurrentTask = CurrentPlan[CurrentPlanStep];
	if (!CurrentTask)
	{
		return EBehaviorUStatus::Failure;
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
		return EBehaviorUStatus::Failure;
	}

	// Pass Invalid (not Running) as the initial ChildStatus — this is the first
	// execution of this task step, so no child has run yet.
	EBehaviorUStatus Result = CurrentTaskExecution->Execute(Agent, EBehaviorUStatus::Invalid);

	if (Result == EBehaviorUStatus::Success)
	{
		// Move to next step
		CurrentPlanStep++;
		CurrentTaskExecution = nullptr;

		if (CurrentPlanStep >= CurrentPlan.Num())
		{
			return EBehaviorUStatus::Success; // Plan fully executed
		}

		return EBehaviorUStatus::Running;
	}

	return Result;
}

bool UBehaviorUHTNPlanner::DecomposeTask(UBehaviorUHTNTask* Task, TArray<UBehaviorUHTNTask*>& OutPlan, int32 Depth)
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
	for (UBehaviorUBehaviorNode* ChildNode : Task->Children)
	{
		UBehaviorUHTNMethod* Method = Cast<UBehaviorUHTNMethod>(ChildNode);
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
		TArray<UBehaviorUHTNTask*> MethodPlan;
		bool bMethodSuccess = true;

		for (UBehaviorUBehaviorNode* SubNode : Method->Children)
		{
			UBehaviorUHTNTask* SubTask = Cast<UBehaviorUHTNTask>(SubNode);
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
