// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "FSM/BehaviorUFSM.h"
#include "BehaviorUAgent.h"

// ===================================================================
// FSM TRANSITIONS
// ===================================================================

UBehaviorUFSMTransition::UBehaviorUFSMTransition()
	: TargetStateId(-1)
{
}

UBehaviorUWaitTransition::UBehaviorUWaitTransition()
	: WaitDuration(1.0f)
{
}

bool UBehaviorUFSMTransition::Evaluate(UBehaviorUAgentComponent* Agent) const
{
	return false;
}

void UBehaviorUFSMTransition::LoadFromProperties(const TArray<FBehaviorUProperty>& Properties)
{
	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("TargetStateId"))
		{
			TargetStateId = FCString::Atoi(*Prop.Value);
		}
	}
}

bool UBehaviorUTransitionCondition::Evaluate(UBehaviorUAgentComponent* Agent) const
{
	if (!Agent) return false;

	FString LeftStr = LeftOperand;
	FString RightStr = RightOperand;

	if (LeftStr.StartsWith(TEXT("Self.")))
		LeftStr = Agent->GetPropertyValue(LeftStr);
	if (RightStr.StartsWith(TEXT("Self.")))
		RightStr = Agent->GetPropertyValue(RightStr);

	// FVector comparison: detect "X= Y= Z=" format, compare by SizeSquared
	// for Greater/Less series (consistent with Condition and Precondition).
	if (BehaviorUIsVectorString(LeftStr) && BehaviorUIsVectorString(RightStr))
	{
		const FVector LeftVec  = BehaviorUStringToVector(LeftStr);
		const FVector RightVec = BehaviorUStringToVector(RightStr);

		switch (Operator)
		{
		case EBehaviorUOperatorType::Equal:			return LeftVec.Equals(RightVec);
		case EBehaviorUOperatorType::NotEqual:		return !LeftVec.Equals(RightVec);
		case EBehaviorUOperatorType::Greater:		return LeftVec.SizeSquared() > RightVec.SizeSquared();
		case EBehaviorUOperatorType::Less:			return LeftVec.SizeSquared() < RightVec.SizeSquared();
		case EBehaviorUOperatorType::GreaterEqual:	return LeftVec.SizeSquared() >= RightVec.SizeSquared();
		case EBehaviorUOperatorType::LessEqual:		return LeftVec.SizeSquared() <= RightVec.SizeSquared();
		default: return false;
		}
	}

	if (LeftStr.IsNumeric() && RightStr.IsNumeric())
	{
		double Left = FCString::Atod(*LeftStr);
		double Right = FCString::Atod(*RightStr);

		switch (Operator)
		{
		case EBehaviorUOperatorType::Equal:			return FMath::IsNearlyEqual(Left, Right);
		case EBehaviorUOperatorType::NotEqual:		return !FMath::IsNearlyEqual(Left, Right);
		case EBehaviorUOperatorType::Greater:		return Left > Right;
		case EBehaviorUOperatorType::Less:			return Left < Right;
		case EBehaviorUOperatorType::GreaterEqual:	return Left >= Right;
		case EBehaviorUOperatorType::LessEqual:		return Left <= Right;
		default: return false;
		}
	}

	return LeftStr == RightStr;
}

bool UBehaviorUWaitTransition::Evaluate(UBehaviorUAgentComponent* Agent) const
{
	// Time-based transition: fires when WaitDuration has elapsed since the
	// owning state was entered. StartTime is set by ResetTimer() which is
	// called by the FSM when the state is (re-)entered.
	if (WaitDuration <= 0.f)
	{
		return true; // Zero or negative duration = immediate transition
	}

	if (!bTimerStarted)
	{
		return false;
	}

	double CurrentTime = (Agent && Agent->GetWorld())
		? Agent->GetWorld()->GetTimeSeconds()
		: FPlatformTime::Seconds();

	return (CurrentTime - StartTime) >= WaitDuration;
}

void UBehaviorUWaitTransition::LoadFromProperties(const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Properties);

	WaitDuration = 1.0f;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("WaitDuration") || Prop.Name == TEXT("Time"))
		{
			WaitDuration = FCString::Atof(*Prop.Value);
		}
	}
}

void UBehaviorUWaitTransition::ResetTimer(UBehaviorUAgentComponent* Agent)
{
	bTimerStarted = true;
	// Use the SAME clock as Evaluate() — GetTimeSeconds() when a World is
	// available, falling back to FPlatformTime::Seconds() only when Agent
	// or World is null.  Mixing the two clocks (FPlatformTime for start,
	// GetTimeSeconds for current) made the timer never fire.
	StartTime = (Agent && Agent->GetWorld())
		? Agent->GetWorld()->GetTimeSeconds()
		: FPlatformTime::Seconds();
}

// ===================================================================
// FSM STATE
// ===================================================================

UBehaviorUFSMState::UBehaviorUFSMState()
	: StateId(-1)
	, bIsInitialState(false)
	, bIsFinalState(false)
{
}

UBehaviorUBehaviorTask* UBehaviorUFSMState::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUFSMStateTask>(Outer);
}

void UBehaviorUFSMState::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("StateId"))
		{
			StateId = FCString::Atoi(*Prop.Value);
		}
		else if (Prop.Name == TEXT("IsInitial"))
		{
			bIsInitialState = (Prop.Value == TEXT("true"));
		}
		else if (Prop.Name == TEXT("IsFinal"))
		{
			bIsFinalState = (Prop.Value == TEXT("true"));
		}
		else if (Prop.Name == TEXT("EnterAction"))
		{
			EnterAction = Prop.Value;
		}
		else if (Prop.Name == TEXT("ExitAction"))
		{
			ExitAction = Prop.Value;
		}
	}
}

bool UBehaviorUFSMStateTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UBehaviorUFSMState* StateNode = Cast<UBehaviorUFSMState>(Node);

	// Reset all transitions' timers for this state (e.g., WaitTransition timers).
	if (StateNode && Agent)
	{
		for (UBehaviorUFSMTransition* Transition : StateNode->Transitions)
		{
			if (Transition)
			{
				Transition->OnStateEntered(Agent);
			}
		}
	}

	// Enqueue the EnterAction method instead of calling ExecuteMethod directly.
	// FSM tasks execute on the worker thread during Phase 2; ExecuteMethod
	// triggers TS/Blueprint/C++ callbacks that must run on the game thread.
	// Using EnqueueMethodCommand with bNeedResult=false defers execution to
	// the next Phase 1 (game thread), which is the safe path used by Action nodes.
	if (StateNode && !StateNode->EnterAction.IsEmpty() && Agent)
	{
		Agent->EnqueueMethodCommand(StateNode->EnterAction, /*bNeedResult=*/false);
	}

	return true;
}

void UBehaviorUFSMStateTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
	const UBehaviorUFSMState* StateNode = Cast<UBehaviorUFSMState>(Node);

	// Enqueue the ExitAction method (same reason as OnEnter — thread safety).
	if (StateNode && !StateNode->ExitAction.IsEmpty() && Agent)
	{
		Agent->EnqueueMethodCommand(StateNode->ExitAction, /*bNeedResult=*/false);
	}
}

EBehaviorUStatus UBehaviorUFSMStateTask::UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	// FSM states are often leaf-like (no child behavior node).
	// Always route through OnUpdate rather than deferring to SingleChildTask
	// which would return Failure when ChildTask==nullptr.
	return OnUpdate(Agent, ChildStatus);
}

EBehaviorUStatus UBehaviorUFSMStateTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUFSMState* StateNode = Cast<UBehaviorUFSMState>(Node);

	if (StateNode && StateNode->bIsFinalState)
	{
		return EBehaviorUStatus::Success;
	}

	// Execute the child behavior (if any)
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}

	return EBehaviorUStatus::Running;
}

// ===================================================================
// WAIT FRAMES STATE / WAIT STATE
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUWaitFramesState::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUWaitFramesStateTask>(Outer);
}

void UBehaviorUWaitFramesState::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	WaitFrameCount = 1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Frames"))
		{
			WaitFrameCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviorUBehaviorTask* UBehaviorUWaitState::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUWaitStateTask>(Outer);
}

void UBehaviorUWaitState::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	WaitDuration = 1.0f;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Time"))
		{
			WaitDuration = FCString::Atof(*Prop.Value);
		}
	}
}

// ===================================================================
// FSM NODE
// ===================================================================

UBehaviorUFSMNode::UBehaviorUFSMNode()
	: InitialStateId(0)
{
}

UBehaviorUBehaviorTask* UBehaviorUFSMNode::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUFSMTask>(Outer);
}

void UBehaviorUFSMNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("InitialId"))
		{
			InitialStateId = FCString::Atoi(*Prop.Value);
		}
	}
}

// ===================================================================
// FSM TASK
// ===================================================================

UBehaviorUFSMTask::UBehaviorUFSMTask()
	: CurrentStateIndex(-1)
{
}

bool UBehaviorUFSMTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UBehaviorUFSMNode* FSMNode = Cast<UBehaviorUFSMNode>(Node);
	if (!FSMNode)
	{
		return false;
	}

	// Find the initial state
	for (int32 i = 0; i < ChildTasks.Num(); i++)
	{
		if (UBehaviorUBehaviorTask* Task = ChildTasks[i])
		{
			const UBehaviorUFSMState* StateNode = Cast<UBehaviorUFSMState>(Task->GetNode());
			if (StateNode && StateNode->StateId == FSMNode->InitialStateId)
			{
				CurrentStateIndex = i;
				return true;
			}
		}
	}

	// Default to first child
	CurrentStateIndex = ChildTasks.Num() > 0 ? 0 : -1;
	return CurrentStateIndex >= 0;
}

void UBehaviorUFSMTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
	// Exit current state — fire ExitAction via ExitState BEFORE Reset,
	// same fix as the transition path above.
	if (ChildTasks.IsValidIndex(CurrentStateIndex))
	{
		if (UBehaviorUFSMStateTask* StateTask = Cast<UBehaviorUFSMStateTask>(ChildTasks[CurrentStateIndex]))
		{
			StateTask->ExitState(Agent, InStatus);
		}
		ChildTasks[CurrentStateIndex]->Reset(Agent);
	}
}

EBehaviorUStatus UBehaviorUFSMTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	return UpdateFSM(Agent, ChildStatus);
}

EBehaviorUStatus UBehaviorUFSMTask::UpdateFSM(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTasks.IsValidIndex(CurrentStateIndex))
	{
		return EBehaviorUStatus::Failure;
	}

	// Execute current state
	UBehaviorUBehaviorTask* CurrentStateTask = ChildTasks[CurrentStateIndex];
	EBehaviorUStatus StateResult = CurrentStateTask->Execute(Agent, ChildStatus);

	// Check transitions from current state
	const UBehaviorUFSMState* CurrentState = Cast<UBehaviorUFSMState>(CurrentStateTask->GetNode());
	if (CurrentState)
	{
		// Check if this is a final state and it completed
		if (CurrentState->bIsFinalState && StateResult != EBehaviorUStatus::Running)
		{
			return StateResult;
		}

		// Check transitions
		for (UBehaviorUFSMTransition* Transition : CurrentState->Transitions)
		{
			if (Transition && Transition->Evaluate(Agent))
			{
				// Exit current state: fire ExitAction via OnExit BEFORE Reset.
				// Reset() alone does not call OnExit, so the ExitAction
				// (enqueued by FSMStateTask::OnExit) would be silently
				// skipped on every state transition. ExitState() is a
				// public wrapper that calls the protected OnExit.
				if (UBehaviorUFSMStateTask* StateTask = Cast<UBehaviorUFSMStateTask>(CurrentStateTask))
				{
					StateTask->ExitState(Agent, EBehaviorUStatus::Success);
				}
				CurrentStateTask->Reset(Agent);

				// Find and enter target state
				UBehaviorUBehaviorTask* TargetTask = FindStateTaskById(Transition->TargetStateId);
				if (TargetTask)
				{
					for (int32 i = 0; i < ChildTasks.Num(); i++)
					{
						if (ChildTasks[i] == TargetTask)
						{
							CurrentStateIndex = i;
							break;
						}
					}
				}

				return EBehaviorUStatus::Running;
			}
		}
	}

	return StateResult == EBehaviorUStatus::Running ? EBehaviorUStatus::Running : StateResult;
}

UBehaviorUBehaviorTask* UBehaviorUFSMTask::FindStateTaskById(int32 StateId) const
{
	for (UBehaviorUBehaviorTask* Task : ChildTasks)
	{
		if (Task)
		{
			const UBehaviorUFSMState* StateNode = Cast<UBehaviorUFSMState>(Task->GetNode());
			if (StateNode && StateNode->StateId == StateId)
			{
				return Task;
			}
		}
	}
	return nullptr;
}

// ===================================================================
// WAIT FRAMES STATE TASK
// ===================================================================

UBehaviorUWaitFramesStateTask::UBehaviorUWaitFramesStateTask()
	: StartFrame(0)
	, TargetFrames(1)
{
}

bool UBehaviorUWaitFramesStateTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	// Run base state enter (EnterAction, etc.)
	if (!Super::OnEnter(Agent))
	{
		return false;
	}

	const UBehaviorUWaitFramesState* WFNode = Cast<UBehaviorUWaitFramesState>(Node);
	TargetFrames = WFNode ? FMath::Max(1, WFNode->WaitFrameCount) : 1;
	StartFrame = static_cast<int32>(GFrameCounter);
	return true;
}

EBehaviorUStatus UBehaviorUWaitFramesStateTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	int32 Elapsed = static_cast<int32>(GFrameCounter) - StartFrame;
	if (Elapsed >= TargetFrames)
	{
		return EBehaviorUStatus::Success;
	}
	return EBehaviorUStatus::Running;
}

// ===================================================================
// WAIT STATE TASK
// ===================================================================

UBehaviorUWaitStateTask::UBehaviorUWaitStateTask()
	: StartTime(0.0)
	, WaitDuration(1.0f)
{
}

bool UBehaviorUWaitStateTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	if (!Super::OnEnter(Agent))
	{
		return false;
	}

	const UBehaviorUWaitState* WaitNode = Cast<UBehaviorUWaitState>(Node);
	WaitDuration = WaitNode ? FMath::Max(0.0f, WaitNode->WaitDuration) : 1.0f;

	if (UWorld* World = Agent ? Agent->GetWorld() : nullptr)
	{
		StartTime = World->GetTimeSeconds();
	}
	else
	{
		StartTime = FPlatformTime::Seconds();
	}

	return true;
}

EBehaviorUStatus UBehaviorUWaitStateTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	double CurrentTime = 0.0;
	if (UWorld* World = Agent ? Agent->GetWorld() : nullptr)
	{
		CurrentTime = World->GetTimeSeconds();
	}
	else
	{
		CurrentTime = FPlatformTime::Seconds();
	}

	if ((CurrentTime - StartTime) >= WaitDuration)
	{
		return EBehaviorUStatus::Success;
	}
	return EBehaviorUStatus::Running;
}
