// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "FSM/BehaviacFSM.h"
#include "BehaviacAgent.h"

// ===================================================================
// FSM TRANSITIONS
// ===================================================================

UBehaviacFSMTransition::UBehaviacFSMTransition()
	: TargetStateId(-1)
{
}

UBehaviacWaitTransition::UBehaviacWaitTransition()
	: WaitDuration(1.0f)
{
}

bool UBehaviacFSMTransition::Evaluate(UBehaviacAgentComponent* Agent) const
{
	return false;
}

void UBehaviacFSMTransition::LoadFromProperties(const TArray<FBehaviacProperty>& Properties)
{
	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("TargetStateId"))
		{
			TargetStateId = FCString::Atoi(*Prop.Value);
		}
	}
}

bool UBehaviacTransitionCondition::Evaluate(UBehaviacAgentComponent* Agent) const
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
	if (BehaviacIsVectorString(LeftStr) && BehaviacIsVectorString(RightStr))
	{
		const FVector LeftVec  = BehaviacStringToVector(LeftStr);
		const FVector RightVec = BehaviacStringToVector(RightStr);

		switch (Operator)
		{
		case EBehaviacOperatorType::Equal:			return LeftVec.Equals(RightVec);
		case EBehaviacOperatorType::NotEqual:		return !LeftVec.Equals(RightVec);
		case EBehaviacOperatorType::Greater:		return LeftVec.SizeSquared() > RightVec.SizeSquared();
		case EBehaviacOperatorType::Less:			return LeftVec.SizeSquared() < RightVec.SizeSquared();
		case EBehaviacOperatorType::GreaterEqual:	return LeftVec.SizeSquared() >= RightVec.SizeSquared();
		case EBehaviacOperatorType::LessEqual:		return LeftVec.SizeSquared() <= RightVec.SizeSquared();
		default: return false;
		}
	}

	if (LeftStr.IsNumeric() && RightStr.IsNumeric())
	{
		double Left = FCString::Atod(*LeftStr);
		double Right = FCString::Atod(*RightStr);

		switch (Operator)
		{
		case EBehaviacOperatorType::Equal:			return FMath::IsNearlyEqual(Left, Right);
		case EBehaviacOperatorType::NotEqual:		return !FMath::IsNearlyEqual(Left, Right);
		case EBehaviacOperatorType::Greater:		return Left > Right;
		case EBehaviacOperatorType::Less:			return Left < Right;
		case EBehaviacOperatorType::GreaterEqual:	return Left >= Right;
		case EBehaviacOperatorType::LessEqual:		return Left <= Right;
		default: return false;
		}
	}

	return LeftStr == RightStr;
}

bool UBehaviacWaitTransition::Evaluate(UBehaviacAgentComponent* Agent) const
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

void UBehaviacWaitTransition::LoadFromProperties(const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Properties);

	WaitDuration = 1.0f;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("WaitDuration") || Prop.Name == TEXT("Time"))
		{
			WaitDuration = FCString::Atof(*Prop.Value);
		}
	}
}

void UBehaviacWaitTransition::ResetTimer(UBehaviacAgentComponent* Agent)
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

UBehaviacFSMState::UBehaviacFSMState()
	: StateId(-1)
	, bIsInitialState(false)
	, bIsFinalState(false)
{
}

UBehaviacBehaviorTask* UBehaviacFSMState::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacFSMStateTask>(Outer);
}

void UBehaviacFSMState::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
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

bool UBehaviacFSMStateTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	const UBehaviacFSMState* StateNode = Cast<UBehaviacFSMState>(Node);

	// Reset all transitions' timers for this state (e.g., WaitTransition timers).
	if (StateNode && Agent)
	{
		for (UBehaviacFSMTransition* Transition : StateNode->Transitions)
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

void UBehaviacFSMStateTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
	const UBehaviacFSMState* StateNode = Cast<UBehaviacFSMState>(Node);

	// Enqueue the ExitAction method (same reason as OnEnter — thread safety).
	if (StateNode && !StateNode->ExitAction.IsEmpty() && Agent)
	{
		Agent->EnqueueMethodCommand(StateNode->ExitAction, /*bNeedResult=*/false);
	}
}

EBehaviacStatus UBehaviacFSMStateTask::UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	// FSM states are often leaf-like (no child behavior node).
	// Always route through OnUpdate rather than deferring to SingleChildTask
	// which would return Failure when ChildTask==nullptr.
	return OnUpdate(Agent, ChildStatus);
}

EBehaviacStatus UBehaviacFSMStateTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacFSMState* StateNode = Cast<UBehaviacFSMState>(Node);

	if (StateNode && StateNode->bIsFinalState)
	{
		return EBehaviacStatus::Success;
	}

	// Execute the child behavior (if any)
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}

	return EBehaviacStatus::Running;
}

// ===================================================================
// WAIT FRAMES STATE / WAIT STATE
// ===================================================================

UBehaviacBehaviorTask* UBehaviacWaitFramesState::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacWaitFramesStateTask>(Outer);
}

void UBehaviacWaitFramesState::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	WaitFrameCount = 1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Frames"))
		{
			WaitFrameCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviacBehaviorTask* UBehaviacWaitState::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacWaitStateTask>(Outer);
}

void UBehaviacWaitState::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	WaitDuration = 1.0f;

	for (const FBehaviacProperty& Prop : Properties)
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

UBehaviacFSMNode::UBehaviacFSMNode()
	: InitialStateId(0)
{
}

UBehaviacBehaviorTask* UBehaviacFSMNode::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacFSMTask>(Outer);
}

void UBehaviacFSMNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
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

UBehaviacFSMTask::UBehaviacFSMTask()
	: CurrentStateIndex(-1)
{
}

bool UBehaviacFSMTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	const UBehaviacFSMNode* FSMNode = Cast<UBehaviacFSMNode>(Node);
	if (!FSMNode)
	{
		return false;
	}

	// Find the initial state
	for (int32 i = 0; i < ChildTasks.Num(); i++)
	{
		if (UBehaviacBehaviorTask* Task = ChildTasks[i])
		{
			const UBehaviacFSMState* StateNode = Cast<UBehaviacFSMState>(Task->GetNode());
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

void UBehaviacFSMTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
	// Exit current state — fire ExitAction via ExitState BEFORE Reset,
	// same fix as the transition path above.
	if (ChildTasks.IsValidIndex(CurrentStateIndex))
	{
		if (UBehaviacFSMStateTask* StateTask = Cast<UBehaviacFSMStateTask>(ChildTasks[CurrentStateIndex]))
		{
			StateTask->ExitState(Agent, InStatus);
		}
		ChildTasks[CurrentStateIndex]->Reset(Agent);
	}
}

EBehaviacStatus UBehaviacFSMTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	return UpdateFSM(Agent, ChildStatus);
}

EBehaviacStatus UBehaviacFSMTask::UpdateFSM(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTasks.IsValidIndex(CurrentStateIndex))
	{
		return EBehaviacStatus::Failure;
	}

	// Execute current state
	UBehaviacBehaviorTask* CurrentStateTask = ChildTasks[CurrentStateIndex];
	EBehaviacStatus StateResult = CurrentStateTask->Execute(Agent, ChildStatus);

	// Check transitions from current state
	const UBehaviacFSMState* CurrentState = Cast<UBehaviacFSMState>(CurrentStateTask->GetNode());
	if (CurrentState)
	{
		// Check if this is a final state and it completed
		if (CurrentState->bIsFinalState && StateResult != EBehaviacStatus::Running)
		{
			return StateResult;
		}

		// Check transitions
		for (UBehaviacFSMTransition* Transition : CurrentState->Transitions)
		{
			if (Transition && Transition->Evaluate(Agent))
			{
				// Exit current state: fire ExitAction via OnExit BEFORE Reset.
				// Reset() alone does not call OnExit, so the ExitAction
				// (enqueued by FSMStateTask::OnExit) would be silently
				// skipped on every state transition. ExitState() is a
				// public wrapper that calls the protected OnExit.
				if (UBehaviacFSMStateTask* StateTask = Cast<UBehaviacFSMStateTask>(CurrentStateTask))
				{
					StateTask->ExitState(Agent, EBehaviacStatus::Success);
				}
				CurrentStateTask->Reset(Agent);

				// Find and enter target state
				UBehaviacBehaviorTask* TargetTask = FindStateTaskById(Transition->TargetStateId);
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

				return EBehaviacStatus::Running;
			}
		}
	}

	return StateResult == EBehaviacStatus::Running ? EBehaviacStatus::Running : StateResult;
}

UBehaviacBehaviorTask* UBehaviacFSMTask::FindStateTaskById(int32 StateId) const
{
	for (UBehaviacBehaviorTask* Task : ChildTasks)
	{
		if (Task)
		{
			const UBehaviacFSMState* StateNode = Cast<UBehaviacFSMState>(Task->GetNode());
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

UBehaviacWaitFramesStateTask::UBehaviacWaitFramesStateTask()
	: StartFrame(0)
	, TargetFrames(1)
{
}

bool UBehaviacWaitFramesStateTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	// Run base state enter (EnterAction, etc.)
	if (!Super::OnEnter(Agent))
	{
		return false;
	}

	const UBehaviacWaitFramesState* WFNode = Cast<UBehaviacWaitFramesState>(Node);
	TargetFrames = WFNode ? FMath::Max(1, WFNode->WaitFrameCount) : 1;
	StartFrame = static_cast<int32>(GFrameCounter);
	return true;
}

EBehaviacStatus UBehaviacWaitFramesStateTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	int32 Elapsed = static_cast<int32>(GFrameCounter) - StartFrame;
	if (Elapsed >= TargetFrames)
	{
		return EBehaviacStatus::Success;
	}
	return EBehaviacStatus::Running;
}

// ===================================================================
// WAIT STATE TASK
// ===================================================================

UBehaviacWaitStateTask::UBehaviacWaitStateTask()
	: StartTime(0.0)
	, WaitDuration(1.0f)
{
}

bool UBehaviacWaitStateTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	if (!Super::OnEnter(Agent))
	{
		return false;
	}

	const UBehaviacWaitState* WaitNode = Cast<UBehaviacWaitState>(Node);
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

EBehaviacStatus UBehaviacWaitStateTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
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
		return EBehaviacStatus::Success;
	}
	return EBehaviacStatus::Running;
}
