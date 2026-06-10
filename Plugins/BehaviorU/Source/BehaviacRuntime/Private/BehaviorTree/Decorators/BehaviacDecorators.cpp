// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Decorators/BehaviacDecorators.h"
#include "BehaviacAgent.h"

DEFINE_LOG_CATEGORY_STATIC(LogBehaviacDecorator, Log, All);

// ===================================================================
// BASE DECORATOR TASK
// ===================================================================

EBehaviacStatus UBehaviacDecoratorTask::UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	// Route through OnUpdate so DecorateResult is applied.
	// (UBehaviacSingleChildTask::UpdateCurrent skips OnUpdate, so we override here.)
	return OnUpdate(Agent, ChildStatus);
}

EBehaviacStatus UBehaviacDecoratorTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask)
	{
		return EBehaviacStatus::Failure;
	}

	EBehaviacStatus Result = ChildTask->Execute(Agent, ChildStatus);
	return DecorateResult(Result);
}

EBehaviacStatus UBehaviacDecoratorTask::DecorateResult(EBehaviacStatus ChildResult)
{
	return ChildResult;
}

// ===================================================================
// AlwaysFailure
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorAlwaysFailure::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorAlwaysFailureTask>(Outer);
}

EBehaviacStatus UBehaviacDecoratorAlwaysFailureTask::DecorateResult(EBehaviacStatus ChildResult)
{
	if (ChildResult == EBehaviacStatus::Running)
		return EBehaviacStatus::Running;
	return EBehaviacStatus::Failure;
}

// ===================================================================
// AlwaysRunning
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorAlwaysRunning::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorAlwaysRunningTask>(Outer);
}

EBehaviacStatus UBehaviacDecoratorAlwaysRunningTask::DecorateResult(EBehaviacStatus ChildResult)
{
	return EBehaviacStatus::Running;
}

// ===================================================================
// AlwaysSuccess
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorAlwaysSuccess::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorAlwaysSuccessTask>(Outer);
}

EBehaviacStatus UBehaviacDecoratorAlwaysSuccessTask::DecorateResult(EBehaviacStatus ChildResult)
{
	if (ChildResult == EBehaviacStatus::Running)
		return EBehaviacStatus::Running;
	return EBehaviacStatus::Success;
}

// ===================================================================
// Not (Inverter)
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorNot::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorNotTask>(Outer);
}

EBehaviacStatus UBehaviacDecoratorNotTask::DecorateResult(EBehaviacStatus ChildResult)
{
	if (ChildResult == EBehaviacStatus::Success) return EBehaviacStatus::Failure;
	if (ChildResult == EBehaviacStatus::Failure) return EBehaviacStatus::Success;
	return ChildResult;
}

// ===================================================================
// Loop
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorLoop::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorLoopTask>(Outer);
}

void UBehaviacDecoratorLoop::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	LoopCount = -1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			LoopCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviacDecoratorLoopTask::UBehaviacDecoratorLoopTask()
	: CurrentCount(0), TargetCount(-1)
{
}

bool UBehaviacDecoratorLoopTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	const UBehaviacDecoratorLoop* LoopNode = Cast<UBehaviacDecoratorLoop>(Node);
	TargetCount = LoopNode ? LoopNode->LoopCount : -1;
	CurrentCount = 0;
	return true;
}

EBehaviacStatus UBehaviacDecoratorLoopTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask)
	{
		return EBehaviacStatus::Failure;
	}

	EBehaviacStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviacStatus::Running)
	{
		return EBehaviacStatus::Running;
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
	return EBehaviacStatus::Running;
}

// ===================================================================
// LoopUntil
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorLoopUntil::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorLoopUntilTask>(Outer);
}

void UBehaviacDecoratorLoopUntil::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bUntilSuccess = true;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Until"))
		{
			bUntilSuccess = (Prop.Value == TEXT("true"));
		}
	}
}

EBehaviacStatus UBehaviacDecoratorLoopUntilTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask)
	{
		return EBehaviacStatus::Failure;
	}

	const UBehaviacDecoratorLoopUntil* LUNode = Cast<UBehaviacDecoratorLoopUntil>(Node);
	bool bUntilSuccess = LUNode ? LUNode->bUntilSuccess : true;

	EBehaviacStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviacStatus::Running)
	{
		return EBehaviacStatus::Running;
	}

	bool bShouldStop = bUntilSuccess ?
		(Result == EBehaviacStatus::Success) : (Result == EBehaviacStatus::Failure);

	if (bShouldStop)
	{
		return Result;
	}

	// Keep looping
	ChildTask->Reset(Agent);
	return EBehaviacStatus::Running;
}

// ===================================================================
// Repeat
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorRepeat::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorRepeatTask>(Outer);
}

void UBehaviacDecoratorRepeat::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	RepeatCount = 1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			RepeatCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviacDecoratorRepeatTask::UBehaviacDecoratorRepeatTask()
	: CurrentCount(0)
{
}

bool UBehaviacDecoratorRepeatTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	CurrentCount = 0;
	return true;
}

EBehaviacStatus UBehaviacDecoratorRepeatTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask) return EBehaviacStatus::Failure;

	const UBehaviacDecoratorRepeat* RepeatNode = Cast<UBehaviacDecoratorRepeat>(Node);
	int32 Target = RepeatNode ? RepeatNode->RepeatCount : 1;

	EBehaviacStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviacStatus::Running) return EBehaviacStatus::Running;
	if (Result == EBehaviacStatus::Failure) return EBehaviacStatus::Failure;

	CurrentCount++;
	if (CurrentCount >= Target)
	{
		return EBehaviacStatus::Success;
	}

	ChildTask->Reset(Agent);
	return EBehaviacStatus::Running;
}

// ===================================================================
// Count
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorCount::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorCountTask>(Outer);
}

void UBehaviacDecoratorCount::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	CountLimit = -1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			CountLimit = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviacDecoratorCountTask::UBehaviacDecoratorCountTask() : CurrentCount(0) {}

bool UBehaviacDecoratorCountTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	CurrentCount++;
	return true;
}

EBehaviacStatus UBehaviacDecoratorCountTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask) return EBehaviacStatus::Failure;
	return ChildTask->Execute(Agent, ChildStatus);
}

// ===================================================================
// CountLimit
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorCountLimit::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorCountLimitTask>(Outer);
}

void UBehaviacDecoratorCountLimit::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	CountMax = 1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			CountMax = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviacDecoratorCountLimitTask::UBehaviacDecoratorCountLimitTask() : ExecutionCount(0) {}

bool UBehaviacDecoratorCountLimitTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	const UBehaviacDecoratorCountLimit* CLNode = Cast<UBehaviacDecoratorCountLimit>(Node);
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

UBehaviacBehaviorTask* UBehaviacDecoratorTime::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorTimeTask>(Outer);
}

void UBehaviacDecoratorTime::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	TimeDuration = 1.0f;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Time"))
		{
			TimeDuration = FCString::Atof(*Prop.Value);
		}
	}
}

bool UBehaviacDecoratorTimeTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	StartTime = Agent ? Agent->GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();
	return true;
}

EBehaviacStatus UBehaviacDecoratorTimeTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask) return EBehaviacStatus::Failure;

	const UBehaviacDecoratorTime* TimeNode = Cast<UBehaviacDecoratorTime>(Node);
	float Duration = TimeNode ? TimeNode->TimeDuration : 1.0f;

	double CurrentTime = Agent ? Agent->GetWorld()->GetTimeSeconds() : FPlatformTime::Seconds();

	// Time expired: stop ticking child and succeed
	if ((CurrentTime - StartTime) >= Duration)
	{
		return EBehaviacStatus::Success;
	}

	// Still within time window: keep ticking child, propagate its result
	return ChildTask->Execute(Agent, ChildStatus);
}

// ===================================================================
// Frames
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorFrames::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorFramesTask>(Outer);
}

void UBehaviacDecoratorFrames::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	FrameCount = 1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Frames"))
		{
			FrameCount = FCString::Atoi(*Prop.Value);
		}
	}
}

bool UBehaviacDecoratorFramesTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	StartFrame = static_cast<int32>(GFrameCounter);
	return true;
}

EBehaviacStatus UBehaviacDecoratorFramesTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask) return EBehaviacStatus::Failure;

	const UBehaviacDecoratorFrames* FramesNode = Cast<UBehaviacDecoratorFrames>(Node);
	int32 Target = FramesNode ? FramesNode->FrameCount : 1;
	int32 Elapsed = static_cast<int32>(GFrameCounter) - StartFrame;

	if (Elapsed >= Target)
	{
		return EBehaviacStatus::Success;
	}

	ChildTask->Execute(Agent, ChildStatus);
	return EBehaviacStatus::Running;
}

// ===================================================================
// FailureUntil
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorFailureUntil::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorFailureUntilTask>(Outer);
}

void UBehaviacDecoratorFailureUntil::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	UntilCount = 1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			UntilCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviacDecoratorFailureUntilTask::UBehaviacDecoratorFailureUntilTask() : CurrentCount(0) {}

bool UBehaviacDecoratorFailureUntilTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	CurrentCount = 0;
	return true;
}

EBehaviacStatus UBehaviacDecoratorFailureUntilTask::DecorateResult(EBehaviacStatus ChildResult)
{
	const UBehaviacDecoratorFailureUntil* FUNode = Cast<UBehaviacDecoratorFailureUntil>(Node);
	int32 Target = FUNode ? FUNode->UntilCount : 1;

	if (ChildResult != EBehaviacStatus::Running)
	{
		CurrentCount++;
		if (CurrentCount >= Target)
		{
			return ChildResult;
		}
		return EBehaviacStatus::Failure;
	}
	return EBehaviacStatus::Running;
}

// ===================================================================
// SuccessUntil
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorSuccessUntil::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorSuccessUntilTask>(Outer);
}

void UBehaviacDecoratorSuccessUntil::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	UntilCount = 1;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Count"))
		{
			UntilCount = FCString::Atoi(*Prop.Value);
		}
	}
}

UBehaviacDecoratorSuccessUntilTask::UBehaviacDecoratorSuccessUntilTask() : CurrentCount(0) {}

bool UBehaviacDecoratorSuccessUntilTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	CurrentCount = 0;
	return true;
}

EBehaviacStatus UBehaviacDecoratorSuccessUntilTask::DecorateResult(EBehaviacStatus ChildResult)
{
	const UBehaviacDecoratorSuccessUntil* SUNode = Cast<UBehaviacDecoratorSuccessUntil>(Node);
	int32 Target = SUNode ? SUNode->UntilCount : 1;

	if (ChildResult != EBehaviacStatus::Running)
	{
		CurrentCount++;
		if (CurrentCount >= Target)
		{
			return ChildResult;
		}
		return EBehaviacStatus::Success;
	}
	return EBehaviacStatus::Running;
}

// ===================================================================
// Iterator
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorIterator::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorIteratorTask>(Outer);
}

void UBehaviacDecoratorIterator::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Opl"))
		{
			ArrayProperty = Prop.Value;
		}
	}
}

UBehaviacDecoratorIteratorTask::UBehaviacDecoratorIteratorTask()
	: CurrentIndex(0), ArrayCount(0)
{
}

bool UBehaviacDecoratorIteratorTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	CurrentIndex = 0;
	// Get array count from agent
	const UBehaviacDecoratorIterator* IterNode = Cast<UBehaviacDecoratorIterator>(Node);
	if (IterNode && Agent)
	{
		FString CountStr = Agent->GetPropertyValue(IterNode->ArrayProperty + TEXT(".Count"));
		ArrayCount = CountStr.IsNumeric() ? FCString::Atoi(*CountStr) : 0;
	}
	return ArrayCount > 0;
}

EBehaviacStatus UBehaviacDecoratorIteratorTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	if (!ChildTask) return EBehaviacStatus::Failure;

	EBehaviacStatus Result = ChildTask->Execute(Agent, ChildStatus);

	if (Result == EBehaviacStatus::Running) return EBehaviacStatus::Running;

	CurrentIndex++;
	if (CurrentIndex >= ArrayCount)
	{
		return Result;
	}

	ChildTask->Reset(Agent);
	return EBehaviacStatus::Running;
}

// ===================================================================
// Log
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorLog::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorLogTask>(Outer);
}

void UBehaviacDecoratorLog::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Log"))
		{
			LogMessage = Prop.Value;
		}
	}
}

EBehaviacStatus UBehaviacDecoratorLogTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacDecoratorLog* LogNode = Cast<UBehaviacDecoratorLog>(Node);
	if (LogNode)
	{
		UE_LOG(LogBehaviacDecorator, Log, TEXT("[Behaviac] %s"), *LogNode->LogMessage);
	}

	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}

	return EBehaviacStatus::Success;
}

// ===================================================================
// Weight
// ===================================================================

UBehaviacBehaviorTask* UBehaviacDecoratorWeight::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacDecoratorWeightTask>(Outer);
}

void UBehaviacDecoratorWeight::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Weight = 1.0f;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Weight"))
		{
			Weight = FCString::Atof(*Prop.Value);
		}
	}
}

EBehaviacStatus UBehaviacDecoratorWeightTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	// Weight decorator just passes through to child; the weight is used by parent SelectorProbability
	if (ChildTask)
	{
		return ChildTask->Execute(Agent, ChildStatus);
	}
	return EBehaviacStatus::Failure;
}
