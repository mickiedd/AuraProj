// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#include "AutoTestNodes.h"
#include "AutoTestAgent.h"
#include "AutoTestLog.h"
#include "BehaviorUTypes.h"
#include "BehaviorUAgent.h"

#include "Engine/World.h"
#include "HAL/PlatformTime.h"

// ===================================================================
// Shared comparison helper.
// ===================================================================

static EAutoTestCompareOp ParseCompareOp(const FString& Op)
{
	if (Op == TEXT("Equals"))			return EAutoTestCompareOp::Equals;
	if (Op == TEXT("NotEquals"))		return EAutoTestCompareOp::NotEquals;
	if (Op == TEXT("GreaterThan"))		return EAutoTestCompareOp::GreaterThan;
	if (Op == TEXT("LessThan"))			return EAutoTestCompareOp::LessThan;
	if (Op == TEXT("GreaterEqual"))		return EAutoTestCompareOp::GreaterEqual;
	if (Op == TEXT("LessEqual"))		return EAutoTestCompareOp::LessEqual;
	return EAutoTestCompareOp::Equals;
}

/** Resolve an operand: literal value used as-is, "Self.X" read from the blackboard. */
static FString ResolveOperand(const FString& In, const UBehaviorUAgentComponent* Agent)
{
	if (In.StartsWith(TEXT("Self.")))
	{
		return Agent ? Agent->GetPropertyValue(In) : FString();
	}
	return In;
}

/**
 * Evaluate Key <Op> Value against the agent's blackboard.
 * Numeric comparison when both sides parse as doubles; otherwise string equality
 * (Equals/NotEquals only — ordering ops on non-numeric values return false).
 */
static bool EvaluateCondition(const UBehaviorUAgentComponent* Agent, const FString& Key, const FString& OpStr, const FString& ValueStr)
{
	if (!Agent)
	{
		return false;
	}

	const FString LeftRaw = Agent->GetPropertyValue(Key);
	const FString RightRaw = ResolveOperand(ValueStr, Agent);

	const EAutoTestCompareOp Op = ParseCompareOp(OpStr);

	// Try numeric comparison first.
	const bool bLeftNumeric = LeftRaw.IsNumeric();
	const bool bRightNumeric = RightRaw.IsNumeric();
	if (bLeftNumeric && bRightNumeric)
	{
		const double L = FCString::Atod(*LeftRaw);
		const double R = FCString::Atod(*RightRaw);
		switch (Op)
		{
		case EAutoTestCompareOp::Equals:			return FMath::IsNearlyEqual(L, R, 1e-6);
		case EAutoTestCompareOp::NotEquals:		return !FMath::IsNearlyEqual(L, R, 1e-6);
		case EAutoTestCompareOp::GreaterThan:	return L > R;
		case EAutoTestCompareOp::LessThan:		return L < R;
		case EAutoTestCompareOp::GreaterEqual:	return L >= R;
		case EAutoTestCompareOp::LessEqual:		return L <= R;
		}
		return false;
	}

	// String comparison (equality only is meaningful for non-numeric values).
	switch (Op)
	{
	case EAutoTestCompareOp::Equals:			return LeftRaw == RightRaw;
	case EAutoTestCompareOp::NotEquals:		return LeftRaw != RightRaw;
	default:								return false; // ordering on non-numeric is undefined
	}
}

/** Format a human-readable description of a failed condition for the assertion message. */
static FString FormatConditionMessage(const FString& Key, const FString& Op, const FString& Value, const UBehaviorUAgentComponent* Agent)
{
	const FString LeftVal = Agent ? Agent->GetPropertyValue(Key) : FString();
	const FString RightVal = ResolveOperand(Value, Agent);
	return FString::Printf(TEXT("%s %s %s  (was: '%s' vs '%s')"), *Key, *Op, *Value, *LeftVal, *RightVal);
}

/** Record an assertion into the agent's run context (if present). */
static void RecordAssertion(UAutoTestAgent* Agent, const FString& Message, bool bPassed)
{
	if (Agent && Agent->GetRunContext())
	{
		Agent->GetRunContext()->AddAssertion(Message, bPassed);
	}
}

// ===================================================================
// ASSERT
// ===================================================================

UBehaviorUBehaviorTask* UAutoTestAssertNode::CreateTask(UObject* Outer) const
{
	return NewObject<UAutoTestAssertTask>(Outer);
}

void UAutoTestAssertNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Key"))			Key = Prop.Value;
		else if (Prop.Name == TEXT("Op"))		Op = Prop.Value;
		else if (Prop.Name == TEXT("Value"))	Value = Prop.Value;
		else if (Prop.Name == TEXT("Message"))	Message = Prop.Value;
	}
}

bool UAutoTestAssertTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UAutoTestAssertNode* NodeDef = Cast<UAutoTestAssertNode>(Node);
	if (!NodeDef || !Agent)
	{
		return false;
	}

	const bool bPassed = EvaluateCondition(Agent, NodeDef->Key, NodeDef->Op, NodeDef->Value);

	UAutoTestAgent* TestAgent = Cast<UAutoTestAgent>(Agent);
	const FString Msg = NodeDef->Message.IsEmpty()
		? FormatConditionMessage(NodeDef->Key, NodeDef->Op, NodeDef->Value, Agent)
		: NodeDef->Message;

	RecordAssertion(TestAgent, Msg, bPassed);
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Assert %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Msg);

	if (!bPassed && TestAgent && TestAgent->GetRunContext())
	{
		TestAgent->GetRunContext()->SetTerminal(EAutoTestStatus::Fail);
	}

	// Returning false ends this node with Failure; the tree continues and the runner
	// observes the terminal status we just set.
	return bPassed;
}

// ===================================================================
// EXPECT
// ===================================================================

UBehaviorUBehaviorTask* UAutoTestExpectNode::CreateTask(UObject* Outer) const
{
	return NewObject<UAutoTestExpectTask>(Outer);
}

void UAutoTestExpectNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bFatal = false;
	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Key"))			Key = Prop.Value;
		else if (Prop.Name == TEXT("Op"))		Op = Prop.Value;
		else if (Prop.Name == TEXT("Value"))	Value = Prop.Value;
		else if (Prop.Name == TEXT("Message"))	Message = Prop.Value;
		else if (Prop.Name == TEXT("Fatal"))	bFatal = (Prop.Value == TEXT("true"));
	}
}

bool UAutoTestExpectTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UAutoTestExpectNode* NodeDef = Cast<UAutoTestExpectNode>(Node);
	if (!NodeDef || !Agent)
	{
		return false;
	}

	const bool bPassed = EvaluateCondition(Agent, NodeDef->Key, NodeDef->Op, NodeDef->Value);

	UAutoTestAgent* TestAgent = Cast<UAutoTestAgent>(Agent);
	const FString Msg = NodeDef->Message.IsEmpty()
		? FormatConditionMessage(NodeDef->Key, NodeDef->Op, NodeDef->Value, Agent)
		: NodeDef->Message;

	RecordAssertion(TestAgent, Msg, bPassed);
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Expect %s: %s"), bPassed ? TEXT("PASS") : TEXT("FAIL"), *Msg);

	if (!bPassed && NodeDef->bFatal && TestAgent && TestAgent->GetRunContext())
	{
		TestAgent->GetRunContext()->SetTerminal(EAutoTestStatus::Fail);
	}

	// Expect always returns Success (soft check) unless Fatal, in which case we already
	// set terminal — return true so the tree keeps stepping; the runner stops it.
	return true;
}

// ===================================================================
// WAIT FOR PROPERTY
// ===================================================================

UBehaviorUBehaviorTask* UAutoTestWaitForPropertyNode::CreateTask(UObject* Outer) const
{
	return NewObject<UAutoTestWaitForPropertyTask>(Outer);
}

void UAutoTestWaitForPropertyNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Timeout = 0.0f;
	bFailOnTimeout = true;
	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Key"))				Key = Prop.Value;
		else if (Prop.Name == TEXT("Op"))			Op = Prop.Value;
		else if (Prop.Name == TEXT("Value"))		Value = Prop.Value;
		else if (Prop.Name == TEXT("Timeout"))		Timeout = FCString::Atof(*Prop.Value);
		else if (Prop.Name == TEXT("FailOnTimeout"))	bFailOnTimeout = (Prop.Value == TEXT("true"));
		else if (Prop.Name == TEXT("Message"))		Message = Prop.Value;
	}
}

void UAutoTestWaitForPropertyTask::Reset(UBehaviorUAgentComponent* Agent)
{
	bStarted = false;
	StartTime = 0.0;
	Super::Reset(Agent);
}

bool UAutoTestWaitForPropertyTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	bStarted = false;
	StartTime = 0.0;
	return true;
}

EBehaviorUStatus UAutoTestWaitForPropertyTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UAutoTestWaitForPropertyNode* NodeDef = Cast<UAutoTestWaitForPropertyNode>(Node);
	if (!NodeDef || !Agent)
	{
		return EBehaviorUStatus::Failure;
	}

	if (!bStarted)
	{
		bStarted = true;
		if (const UWorld* World = Agent->GetWorld())
		{
			StartTime = World->GetTimeSeconds();
		}
		else
		{
			StartTime = FPlatformTime::Seconds();
		}
	}

	if (EvaluateCondition(Agent, NodeDef->Key, NodeDef->Op, NodeDef->Value))
	{
		return EBehaviorUStatus::Success;
	}

	// Timeout check.
	if (NodeDef->Timeout > 0.0f)
	{
		double CurrentTime = 0.0;
		if (const UWorld* World = Agent->GetWorld())
		{
			CurrentTime = World->GetTimeSeconds();
		}
		else
		{
			CurrentTime = FPlatformTime::Seconds();
		}

		if (CurrentTime - StartTime >= NodeDef->Timeout)
		{
			const FString Msg = NodeDef->Message.IsEmpty()
				? FString::Printf(TEXT("WaitForProperty timed out: %s %s %s"), *NodeDef->Key, *NodeDef->Op, *NodeDef->Value)
				: NodeDef->Message;

			if (NodeDef->bFailOnTimeout)
			{
				UAutoTestAgent* TestAgent = Cast<UAutoTestAgent>(Agent);
				RecordAssertion(TestAgent, Msg, false);
				UE_LOG(LogAuraTest, Warning, TEXT("[AutoTest] WaitForProperty TIMEOUT (fail): %s"), *Msg);
				if (TestAgent && TestAgent->GetRunContext())
				{
					TestAgent->GetRunContext()->SetTerminal(EAutoTestStatus::Fail);
				}
				return EBehaviorUStatus::Failure;
			}
			UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] WaitForProperty timeout (non-fatal): %s"), *Msg);
			return EBehaviorUStatus::Success;
		}
	}

	return EBehaviorUStatus::Running;
}

// ===================================================================
// SPAWN ACTOR
// ===================================================================

UBehaviorUBehaviorTask* UAutoTestSpawnActorNode::CreateTask(UObject* Outer) const
{
	return NewObject<UAutoTestSpawnActorTask>(Outer);
}

void UAutoTestSpawnActorNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	MethodName = TEXT("SpawnActor");
	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Class"))			ClassPath = Prop.Value;
		else if (Prop.Name == TEXT("Location"))	Location = Prop.Value;
		else if (Prop.Name == TEXT("OutputKey"))	OutputKey = Prop.Value;
		else if (Prop.Name == TEXT("Method"))	MethodName = Prop.Value;
	}
}

EBehaviorUStatus UAutoTestSpawnActorTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UAutoTestSpawnActorNode* NodeDef = Cast<UAutoTestSpawnActorNode>(Node);
	if (!NodeDef || !Agent)
	{
		return EBehaviorUStatus::Failure;
	}

	// First call: stash config into fixed blackboard keys, then enqueue the spawn
	// command (executed on the game thread next frame by the agent's method handler).
	if (!bHasPendingCommand)
	{
		Agent->SetPropertyValue(UAutoTestAgent::SpawnConfigClassKey(), NodeDef->ClassPath);
		Agent->SetPropertyValue(UAutoTestAgent::SpawnConfigLocationKey(), NodeDef->Location);
		Agent->SetPropertyValue(UAutoTestAgent::SpawnConfigOutputKey(), NodeDef->OutputKey);

		PendingCommandId = Agent->EnqueueMethodCommand(NodeDef->MethodName, /*bNeedResult=*/true);
		bHasPendingCommand = true;
		return EBehaviorUStatus::Running;
	}

	// Subsequent calls: read the spawn result.
	EBehaviorUStatus Result = EBehaviorUStatus::Invalid;
	if (Agent->GetCommandResult(PendingCommandId, Result))
	{
		bHasPendingCommand = false;
		PendingCommandId = 0;
		UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] SpawnActor result=%d"), (int32)Result);
		return (Result == EBehaviorUStatus::Invalid) ? EBehaviorUStatus::Failure : Result;
	}

	return EBehaviorUStatus::Running;
}

void UAutoTestSpawnActorTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
	if (bHasPendingCommand && Agent)
	{
		Agent->ClearCommandResult(PendingCommandId);
		bHasPendingCommand = false;
		PendingCommandId = 0;
	}
	Super::OnExit(Agent, InStatus);
}

void UAutoTestSpawnActorTask::Reset(UBehaviorUAgentComponent* Agent)
{
	if (bHasPendingCommand && Agent)
	{
		Agent->ClearCommandResult(PendingCommandId);
	}
	bHasPendingCommand = false;
	PendingCommandId = 0;
	Super::Reset(Agent);
}

// ===================================================================
// LOG RESULT
// ===================================================================

UBehaviorUBehaviorTask* UAutoTestLogResultNode::CreateTask(UObject* Outer) const
{
	return NewObject<UAutoTestLogResultTask>(Outer);
}

void UAutoTestLogResultNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bPassed = true;
	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Message"))	Message = Prop.Value;
		else if (Prop.Name == TEXT("Passed"))	bPassed = (Prop.Value == TEXT("true"));
	}
}

bool UAutoTestLogResultTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UAutoTestLogResultNode* NodeDef = Cast<UAutoTestLogResultNode>(Node);
	if (!NodeDef)
	{
		return false;
	}

	UAutoTestAgent* TestAgent = Cast<UAutoTestAgent>(Agent);
	RecordAssertion(TestAgent, NodeDef->Message, NodeDef->bPassed);
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Checkpoint %s: %s"), NodeDef->bPassed ? TEXT("OK") : TEXT("BAD"), *NodeDef->Message);
	return true; // Success
}

// ===================================================================
// PASS / FAIL
// ===================================================================

UBehaviorUBehaviorTask* UAutoTestPassNode::CreateTask(UObject* Outer) const
{
	return NewObject<UAutoTestPassTask>(Outer);
}

bool UAutoTestPassTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	UAutoTestAgent* TestAgent = Cast<UAutoTestAgent>(Agent);
	if (TestAgent && TestAgent->GetRunContext())
	{
		TestAgent->GetRunContext()->SetTerminal(EAutoTestStatus::Pass);
	}
	UE_LOG(LogAuraTest, Log, TEXT("[AutoTest] Pass node reached"));
	return true;
}

UBehaviorUBehaviorTask* UAutoTestFailNode::CreateTask(UObject* Outer) const
{
	return NewObject<UAutoTestFailTask>(Outer);
}

void UAutoTestFailNode::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Message"))	Message = Prop.Value;
	}
}

bool UAutoTestFailTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UAutoTestFailNode* NodeDef = Cast<UAutoTestFailNode>(Node);
	UAutoTestAgent* TestAgent = Cast<UAutoTestAgent>(Agent);
	const FString Msg = NodeDef ? NodeDef->Message : TEXT("Fail node reached");

	RecordAssertion(TestAgent, Msg, false);
	UE_LOG(LogAuraTest, Error, TEXT("[AutoTest] Fail node reached: %s"), *Msg);
	if (TestAgent && TestAgent->GetRunContext())
	{
		TestAgent->GetRunContext()->SetTerminal(EAutoTestStatus::Fail);
	}
	return false; // Failure
}