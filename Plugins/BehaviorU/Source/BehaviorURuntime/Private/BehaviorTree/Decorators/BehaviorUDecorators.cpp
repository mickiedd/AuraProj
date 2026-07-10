// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Decorators/BehaviorUDecorators.h"
#include "BehaviorUAgent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBehaviorUDecorator, Log, All);

// ===================================================================
// BASE DECORATOR TASK
// ===================================================================

EBehaviorUStatus UBehaviorUDecoratorTask::UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	// Route through OnUpdate so DecorateResult is applied.
	// (UBehaviorUSingleChildTask::UpdateCurrent skips OnUpdate, so we override here.)
	return OnUpdate(Agent, ChildStatus);
}

EBehaviorUStatus UBehaviorUDecoratorTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask)
	{
		return EBehaviorUStatus::Failure;
	}

	EBehaviorUStatus Result = ChildTask->Execute(Agent, ChildStatus);
	return DecorateResult(Result);
}

EBehaviorUStatus UBehaviorUDecoratorTask::DecorateResult(EBehaviorUStatus ChildResult)
{
	return ChildResult;
}

// ===================================================================
// AlwaysFailure
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorAlwaysFailure::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorAlwaysFailureTask>(Outer);
}

EBehaviorUStatus UBehaviorUDecoratorAlwaysFailureTask::DecorateResult(EBehaviorUStatus ChildResult)
{
	if (ChildResult == EBehaviorUStatus::Running)
		return EBehaviorUStatus::Running;
	return EBehaviorUStatus::Failure;
}

// ===================================================================
// AlwaysRunning
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorAlwaysRunning::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorAlwaysRunningTask>(Outer);
}

EBehaviorUStatus UBehaviorUDecoratorAlwaysRunningTask::DecorateResult(EBehaviorUStatus ChildResult)
{
	return EBehaviorUStatus::Running;
}

// ===================================================================
// AlwaysSuccess
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorAlwaysSuccess::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorAlwaysSuccessTask>(Outer);
}

EBehaviorUStatus UBehaviorUDecoratorAlwaysSuccessTask::DecorateResult(EBehaviorUStatus ChildResult)
{
	if (ChildResult == EBehaviorUStatus::Running)
		return EBehaviorUStatus::Running;
	return EBehaviorUStatus::Success;
}

// ===================================================================
// Not (Inverter)
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorNot::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorNotTask>(Outer);
}

EBehaviorUStatus UBehaviorUDecoratorNotTask::DecorateResult(EBehaviorUStatus ChildResult)
{
	if (ChildResult == EBehaviorUStatus::Success) return EBehaviorUStatus::Failure;
	if (ChildResult == EBehaviorUStatus::Failure) return EBehaviorUStatus::Success;
	return ChildResult;
}

// ===================================================================
// Loop
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorLoop::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorLoopTask>(Outer);
}

void UBehaviorUDecoratorLoop::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	LoopCount = -1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			LoopCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviorUDecoratorLoopTask::UBehaviorUDecoratorLoopTask()
	: CurrentCount(0), TargetCount(-1)
{
}

bool UBehaviorUDecoratorLoopTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UBehaviorUDecoratorLoop* LoopNode = Cast<UBehaviorUDecoratorLoop>(Node);
	TargetCount = LoopNode ? LoopNode->LoopCount : -1;
	CurrentCount = 0;

	// LoopCount == 0 means "don't loop at all" — return false to prevent
	// the child from being entered, which makes OnUpdate return Failure.
	if (TargetCount == 0)
	{
		return false;
	}
	return true;
}

EBehaviorUStatus UBehaviorUDecoratorLoopTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask)
	{
		return EBehaviorUStatus::Failure;
	}

	EBehaviorUStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviorUStatus::Running)
	{
		return EBehaviorUStatus::Running;
	}

	// Child completed, increment counter
	CurrentCount++;

	// Check if we've reached the limit
	if (TargetCount > 0 && CurrentCount >= TargetCount)
	{
		return Result;
	}

	// Reset child for next iteration
	ChildTask->Reset(Agent);
	return EBehaviorUStatus::Running;
}

// ===================================================================
// LoopUntil
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorLoopUntil::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorLoopUntilTask>(Outer);
}

void UBehaviorUDecoratorLoopUntil::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bUntilSuccess = true;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Until"))
		{
			bUntilSuccess = (Prop.Value == TEXT("true"));
		}
	}
}

EBehaviorUStatus UBehaviorUDecoratorLoopUntilTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask)
	{
		return EBehaviorUStatus::Failure;
	}

	const UBehaviorUDecoratorLoopUntil* LUNode = Cast<UBehaviorUDecoratorLoopUntil>(Node);
	bool bUntilSuccess = LUNode ? LUNode->bUntilSuccess : true;

	EBehaviorUStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviorUStatus::Running)
	{
		return EBehaviorUStatus::Running;
	}

	bool bShouldStop = bUntilSuccess ?
		(Result == EBehaviorUStatus::Success) : (Result == EBehaviorUStatus::Failure);

	if (bShouldStop)
	{
		return Result;
	}

	// Keep looping
	ChildTask->Reset(Agent);
	return EBehaviorUStatus::Running;
}

// ===================================================================
// Repeat
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorRepeat::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorRepeatTask>(Outer);
}

void UBehaviorUDecoratorRepeat::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	RepeatCount = 1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			RepeatCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviorUDecoratorRepeatTask::UBehaviorUDecoratorRepeatTask()
	: CurrentCount(0)
{
}

bool UBehaviorUDecoratorRepeatTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	CurrentCount = 0;
	return true;
}

EBehaviorUStatus UBehaviorUDecoratorRepeatTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask) return EBehaviorUStatus::Failure;

	const UBehaviorUDecoratorRepeat* RepeatNode = Cast<UBehaviorUDecoratorRepeat>(Node);
	int32 Target = RepeatNode ? RepeatNode->RepeatCount : 1;

	EBehaviorUStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviorUStatus::Running) return EBehaviorUStatus::Running;
	if (Result == EBehaviorUStatus::Failure) return EBehaviorUStatus::Failure;

	CurrentCount++;
	if (CurrentCount >= Target)
	{
		return EBehaviorUStatus::Success;
	}

	ChildTask->Reset(Agent);
	return EBehaviorUStatus::Running;
}

// ===================================================================
// Count
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorCount::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorCountTask>(Outer);
}

void UBehaviorUDecoratorCount::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	CountLimit = -1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			CountLimit = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviorUDecoratorCountTask::UBehaviorUDecoratorCountTask() : CurrentCount(0) {}

bool UBehaviorUDecoratorCountTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	CurrentCount++;
	return true;
}

EBehaviorUStatus UBehaviorUDecoratorCountTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask) return EBehaviorUStatus::Failure;
	return ChildTask->Execute(Agent, ChildStatus);
}

// ===================================================================
// CountLimit
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorCountLimit::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorCountLimitTask>(Outer);
}

void UBehaviorUDecoratorCountLimit::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	CountMax = 1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			CountMax = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviorUDecoratorCountLimitTask::UBehaviorUDecoratorCountLimitTask() : ExecutionCount(0) {}

bool UBehaviorUDecoratorCountLimitTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UBehaviorUDecoratorCountLimit* CLNode = Cast<UBehaviorUDecoratorCountLimit>(Node);
	int32 Max = CLNode ? CLNode->CountMax : 1;

	if (ExecutionCount >= Max)
	{
		return false; // Block entry
	}

	ExecutionCount++;
	return true;
}

// ===================================================================
// Time
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorTime::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorTimeTask>(Outer);
}

void UBehaviorUDecoratorTime::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	TimeDuration = 1.0f;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Time"))
		{
			TimeDuration = FCString::Atof(*Prop.Value);
		}
	}
}

bool UBehaviorUDecoratorTimeTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	StartTime = Agent ? Agent->GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	return true;
}

EBehaviorUStatus UBehaviorUDecoratorTimeTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask) return EBehaviorUStatus::Failure;

	const UBehaviorUDecoratorTime* TimeNode = Cast<UBehaviorUDecoratorTime>(Node);
	float Duration = TimeNode ? TimeNode->TimeDuration : 1.0f;

	double CurrentTime = Agent ? Agent->GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();

	// Time expired: stop ticking child, reset it so the next entry is clean,
	// and return Success.
	if ((CurrentTime - StartTime) >= Duration)
	{
		ChildTask->Reset(Agent);
		return EBehaviorUStatus::Success;
	}

	// Still within time window: tick child and propagate its result.
	// Previously the child's result was ignored and Running was always
	// returned — now we propagate Success/Failure from the child so the
	// decorator can complete early if the child finishes.
	EBehaviorUStatus ChildResult = ChildTask->Execute(Agent, ChildStatus);
	if (ChildResult == EBehaviorUStatus::Failure)
	{
		return EBehaviorUStatus::Failure;
	}
	// Success or Running: keep running until the timer expires.
	return EBehaviorUStatus::Running;
}

// ===================================================================
// Frames
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorFrames::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorFramesTask>(Outer);
}

void UBehaviorUDecoratorFrames::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	FrameCount = 1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Frames"))
		{
			FrameCount = FCString::Atoi(*Prop.Value);
		}
	}
}

bool UBehaviorUDecoratorFramesTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	StartFrame = static_cast<int32>(GFrameCounter);
	return true;
}

EBehaviorUStatus UBehaviorUDecoratorFramesTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask) return EBehaviorUStatus::Failure;

	const UBehaviorUDecoratorFrames* FramesNode = Cast<UBehaviorUDecoratorFrames>(Node);
	int32 Target = FramesNode ? FramesNode->FrameCount : 1;
	int32 Elapsed = static_cast<int32>(GFrameCounter) - StartFrame;

	if (Elapsed >= Target)
	{
		// Frame limit reached: reset child so next entry is clean, return Success.
		ChildTask->Reset(Agent);
		return EBehaviorUStatus::Success;
	}

	// Still within frame window: tick child and propagate Failure.
	// Previously the child's result was ignored and Running was always
	// returned — now we propagate Failure so the decorator can fail early.
	EBehaviorUStatus ChildResult = ChildTask->Execute(Agent, ChildStatus);
	if (ChildResult == EBehaviorUStatus::Failure)
	{
		return EBehaviorUStatus::Failure;
	}
	// Success or Running: keep running until frame limit is reached.
	return EBehaviorUStatus::Running;
}

// ===================================================================
// FailureUntil
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorFailureUntil::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorFailureUntilTask>(Outer);
}

void UBehaviorUDecoratorFailureUntil::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	UntilCount = 1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			UntilCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviorUDecoratorFailureUntilTask::UBehaviorUDecoratorFailureUntilTask() : CurrentCount(0) {}

bool UBehaviorUDecoratorFailureUntilTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	CurrentCount = 0;
	return true;
}

EBehaviorUStatus UBehaviorUDecoratorFailureUntilTask::DecorateResult(EBehaviorUStatus ChildResult)
{
	const UBehaviorUDecoratorFailureUntil* FUNode = Cast<UBehaviorUDecoratorFailureUntil>(Node);
	int32 Target = FUNode ? FUNode->UntilCount : 1;

	if (ChildResult != EBehaviorUStatus::Running)
	{
		CurrentCount++;
		if (CurrentCount >= Target)
		{
			return ChildResult;
		}
		return EBehaviorUStatus::Failure;
	}
	return EBehaviorUStatus::Running;
}

// ===================================================================
// SuccessUntil
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorSuccessUntil::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorSuccessUntilTask>(Outer);
}

void UBehaviorUDecoratorSuccessUntil::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	UntilCount = 1;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			UntilCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviorUDecoratorSuccessUntilTask::UBehaviorUDecoratorSuccessUntilTask() : CurrentCount(0) {}

bool UBehaviorUDecoratorSuccessUntilTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	CurrentCount = 0;
	return true;
}

EBehaviorUStatus UBehaviorUDecoratorSuccessUntilTask::DecorateResult(EBehaviorUStatus ChildResult)
{
	const UBehaviorUDecoratorSuccessUntil* SUNode = Cast<UBehaviorUDecoratorSuccessUntil>(Node);
	int32 Target = SUNode ? SUNode->UntilCount : 1;

	if (ChildResult != EBehaviorUStatus::Running)
	{
		CurrentCount++;
		if (CurrentCount >= Target)
		{
			return ChildResult;
		}
		return EBehaviorUStatus::Success;
	}
	return EBehaviorUStatus::Running;
}

// ===================================================================
// Iterator
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorIterator::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorIteratorTask>(Outer);
}

void UBehaviorUDecoratorIterator::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Opl"))
		{
			ArrayProperty = Prop.Value;
		}
	}
}

UBehaviorUDecoratorIteratorTask::UBehaviorUDecoratorIteratorTask()
	: CurrentIndex(0), ArrayCount(0)
{
}

bool UBehaviorUDecoratorIteratorTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	CurrentIndex = 0;
	// Get array count from agent
	const UBehaviorUDecoratorIterator* IterNode = Cast<UBehaviorUDecoratorIterator>(Node);
	if (IterNode && Agent)
	{
		FString CountStr = Agent->GetPropertyValue(IterNode->ArrayProperty + TEXT(".Count"));
		ArrayCount = CountStr.IsNumeric() ? FCString::Atoi(*CountStr) : 0;
	}
	return ArrayCount > 0;
}

EBehaviorUStatus UBehaviorUDecoratorIteratorTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	if (!ChildTask) return EBehaviorUStatus::Failure;

	EBehaviorUStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviorUStatus::Running) return EBehaviorUStatus::Running;

	CurrentIndex++;
	if (CurrentIndex >= ArrayCount)
	{
		return Result;
	}

	ChildTask->Reset(Agent);
	return EBehaviorUStatus::Running;
}

// ===================================================================
// Log
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorLog::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorLogTask>(Outer);
}

void UBehaviorUDecoratorLog::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Message"))
		{
			LogMessage = Prop.Value;
		}
	}
}

EBehaviorUStatus UBehaviorUDecoratorLogTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUDecoratorLog* LogNode = Cast<UBehaviorUDecoratorLog>(Node);
	if (LogNode)
	{
		UE_LOG(LogBehaviorUDecorator, Log, TEXT("[BehaviorU] %s"), *LogNode->LogMessage);
	}

	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}

	return EBehaviorUStatus::Success;
}

// ===================================================================
// Weight
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUDecoratorWeight::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUDecoratorWeightTask>(Outer);
}

void UBehaviorUDecoratorWeight::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Weight = 1.0f;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Weight"))
		{
			Weight = FCString::Atof(*Prop.Value);
		}
	}
}

EBehaviorUStatus UBehaviorUDecoratorWeightTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	// Weight decorator just passes through to child; the weight is used by parent SelectorProbability
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}
	return EBehaviorUStatus::Failure;
}
