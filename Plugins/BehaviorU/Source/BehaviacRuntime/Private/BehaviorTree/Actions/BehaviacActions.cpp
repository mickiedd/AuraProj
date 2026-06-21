// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Actions/BehaviacActions.h"
#include "BehaviacAgent.h"

// ===================================================================
// ACTION
// ===================================================================

UBehaviacBehaviorTask* UBehaviacAction::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacActionTask>(Outer);
}

void UBehaviacAction::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	ResultOption = EBehaviacStatus::Success;

	BEHAVIAC_VLOG(TEXT("[Behaviac] Action::LoadFromProperties: Got %d properties"), Properties.Num());

	for (const FBehaviacProperty& Prop : Properties)
	{
		BEHAVIAC_VLOG(TEXT("[Behaviac]    Property: '%s' = '%s'"), *Prop.Name, *Prop.Value);

		if (Prop.Name == TEXT("Method"))
		{
			MethodName = Prop.Value;
			BEHAVIAC_VLOG(TEXT("[Behaviac]    Set MethodName to '%s'"), *MethodName);
		}
		else if (Prop.Name == TEXT("ResultOption"))
		{
			if (Prop.Value == TEXT("BT_SUCCESS")) ResultOption = EBehaviacStatus::Success;
			else if (Prop.Value == TEXT("BT_FAILURE")) ResultOption = EBehaviacStatus::Failure;
			else if (Prop.Value == TEXT("BT_RUNNING")) ResultOption = EBehaviacStatus::Running;
		}
	}

	BEHAVIAC_VLOG(TEXT("[Behaviac] After parsing: MethodName='%s'"), *MethodName);
}

EBehaviacStatus UBehaviacActionTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacAction* ActionNode = Cast<UBehaviacAction>(Node);
	if (!ActionNode || !Agent)
	{
		BEHAVIAC_VLOG(TEXT("[Behaviac] ActionTask::OnUpdate: ActionNode=%d, Agent=%d"),
			ActionNode != nullptr, Agent != nullptr);
		return EBehaviacStatus::Failure;
	}

	// --- 第一次调用：提交命令到队列 ---
	if (!bHasPendingCommand)
	{
		// ResultOption != Running 表示设计者要覆盖最终结果，方法调用为即发即忘
		const bool bNeedResult = (ActionNode->ResultOption == EBehaviacStatus::Running);

		PendingCommandId = Agent->EnqueueMethodCommand(ActionNode->MethodName, bNeedResult);

		BEHAVIAC_VLOG(TEXT("[Behaviac] ActionTask::OnUpdate: 已入队 id=%llu method='%s' needResult=%d"),
			PendingCommandId, *ActionNode->MethodName, (int32)bNeedResult);

		if (!bNeedResult)
		{
			// 即发即忘：方法副作用会在下帧执行，本节点直接返回设计者指定的状态
			PendingCommandId = 0;
			return ActionNode->ResultOption;
		}

		bHasPendingCommand = true;
		return EBehaviacStatus::Running;
	}

	// --- 后续调用：等待上帧命令的执行结果 ---
	EBehaviacStatus Result;
	if (Agent->GetCommandResult(PendingCommandId, Result))
	{
		BEHAVIAC_VLOG(TEXT("[Behaviac] ActionTask::OnUpdate: id=%llu method='%s' 结果=%d"),
			PendingCommandId, *ActionNode->MethodName, (int32)Result);

		bHasPendingCommand = false;
		PendingCommandId   = 0;

		// Running 优先（方法自身请求继续异步执行）
		if (Result == EBehaviacStatus::Running)
		{
			return EBehaviacStatus::Running;
		}

		// ResultOption == Running means "forward the method's real result."
		// If the method returned Invalid (no handler registered), treat it as
		// Failure so missing handlers are visible instead of silently succeeding.
		if (ActionNode->ResultOption == EBehaviacStatus::Running)
		{
			return (Result != EBehaviacStatus::Invalid) ? Result : EBehaviacStatus::Failure;
		}

		return ActionNode->ResultOption;
	}

	// 结果尚未就绪，继续等待
	return EBehaviacStatus::Running;
}

void UBehaviacActionTask::OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus)
{
	// 若树被外部中断（Selector 子节点失败导致退出等情况），清理残留结果条目
	if (bHasPendingCommand && Agent)
	{
		BEHAVIAC_VLOG(TEXT("[Behaviac] ActionTask::OnExit: 中断清理 id=%llu"), PendingCommandId);
		Agent->ClearCommandResult(PendingCommandId);
		bHasPendingCommand = false;
		PendingCommandId   = 0;
	}

	Super::OnExit(Agent, InStatus);
}

void UBehaviacActionTask::Reset(UBehaviacAgentComponent* Agent)
{
	// ResetBehaviorTree() 不经过 OnExit，需在此主动清理挂起命令状态
	if (bHasPendingCommand && Agent)
	{
		UE_LOG(LogBehaviac, Verbose, TEXT("[Behaviac] ActionTask::Reset: 清理残留挂起命令 id=%llu"), PendingCommandId);
		Agent->ClearCommandResult(PendingCommandId);
	}
	bHasPendingCommand = false;
	PendingCommandId   = 0;

	Super::Reset(Agent);
}

// ===================================================================
// ASSIGNMENT
// ===================================================================

UBehaviacBehaviorTask* UBehaviacAssignment::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacAssignmentTask>(Outer);
}

void UBehaviacAssignment::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bCastFromRight = false;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Opl"))
		{
			PropertyName = Prop.Value;
		}
		else if (Prop.Name == TEXT("Opr"))
		{
			PropertyValue = Prop.Value;
		}
		else if (Prop.Name == TEXT("CastRight"))
		{
			bCastFromRight = (Prop.Value == TEXT("true"));
		}
	}
}

EBehaviacStatus UBehaviacAssignmentTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacAssignment* AssignNode = Cast<UBehaviacAssignment>(Node);
	if (!AssignNode || !Agent)
	{
		return EBehaviacStatus::Failure;
	}

	FString Value = AssignNode->PropertyValue;

	// Resolve if it references another property
	if (Value.StartsWith(TEXT("Self.")))
	{
		Value = Agent->GetPropertyValue(Value);
	}

	Agent->SetPropertyValue(AssignNode->PropertyName, Value);
	return EBehaviacStatus::Success;
}

// ===================================================================
// COMPUTE
// ===================================================================

UBehaviacBehaviorTask* UBehaviacCompute::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacComputeTask>(Outer);
}

void UBehaviacCompute::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Operator = EBehaviacOperatorType::Add;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Opl"))
		{
			ResultProperty = Prop.Value;
		}
		else if (Prop.Name == TEXT("Opr1"))
		{
			LeftOperand = Prop.Value;
		}
		else if (Prop.Name == TEXT("Opr2"))
		{
			RightOperand = Prop.Value;
		}
		else if (Prop.Name == TEXT("Operator"))
		{
			if (Prop.Value == TEXT("Add")) Operator = EBehaviacOperatorType::Add;
			else if (Prop.Value == TEXT("Sub")) Operator = EBehaviacOperatorType::Subtract;
			else if (Prop.Value == TEXT("Mul")) Operator = EBehaviacOperatorType::Multiply;
			else if (Prop.Value == TEXT("Div")) Operator = EBehaviacOperatorType::Divide;
		}
	}
}

EBehaviacStatus UBehaviacComputeTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacCompute* ComputeNode = Cast<UBehaviacCompute>(Node);
	if (!ComputeNode || !Agent)
	{
		return EBehaviacStatus::Failure;
	}

	FString LeftStr = ComputeNode->LeftOperand;
	FString RightStr = ComputeNode->RightOperand;

	if (LeftStr.StartsWith(TEXT("Self.")))
	{
		LeftStr = Agent->GetPropertyValue(LeftStr);
	}
	if (RightStr.StartsWith(TEXT("Self.")))
	{
		RightStr = Agent->GetPropertyValue(RightStr);
	}

	// FVector 运算：左操作数为向量时进入此分支
	// 右操作数可为向量（逐分量运算）或标量（乘除缩放）
	if (BehaviacIsVectorString(LeftStr))
	{
		const FVector LeftVec = BehaviacStringToVector(LeftStr);
		FVector VecResult = FVector::ZeroVector;

		if (BehaviacIsVectorString(RightStr))
		{
			// 两侧均为向量：逐分量运算
			const FVector RightVec = BehaviacStringToVector(RightStr);
			switch (ComputeNode->Operator)
			{
			case EBehaviacOperatorType::Add:
				VecResult = LeftVec + RightVec; break;
			case EBehaviacOperatorType::Subtract:
				VecResult = LeftVec - RightVec; break;
			case EBehaviacOperatorType::Multiply:
				VecResult = LeftVec * RightVec; break;
			case EBehaviacOperatorType::Divide:
				VecResult = FVector(
					RightVec.X != 0.f ? LeftVec.X / RightVec.X : 0.f,
					RightVec.Y != 0.f ? LeftVec.Y / RightVec.Y : 0.f,
					RightVec.Z != 0.f ? LeftVec.Z / RightVec.Z : 0.f); break;
			default: break;
			}
		}
		else
		{
			// 右侧为标量：缩放向量
			const float Scalar = FCString::Atof(*RightStr);
			switch (ComputeNode->Operator)
			{
			case EBehaviacOperatorType::Add:
				VecResult = LeftVec + FVector(Scalar); break;
			case EBehaviacOperatorType::Subtract:
				VecResult = LeftVec - FVector(Scalar); break;
			case EBehaviacOperatorType::Multiply:
				VecResult = LeftVec * Scalar; break;
			case EBehaviacOperatorType::Divide:
				VecResult = (Scalar != 0.f) ? LeftVec / Scalar : FVector::ZeroVector; break;
			default: break;
			}
		}

		Agent->SetPropertyValue(ComputeNode->ResultProperty, BehaviacVectorToString(VecResult));
		return EBehaviacStatus::Success;
	}

	double Left = FCString::Atod(*LeftStr);
	double Right = FCString::Atod(*RightStr);
	double Result = 0.0;

	switch (ComputeNode->Operator)
	{
	case EBehaviacOperatorType::Add:		Result = Left + Right; break;
	case EBehaviacOperatorType::Subtract:	Result = Left - Right; break;
	case EBehaviacOperatorType::Multiply:	Result = Left * Right; break;
	case EBehaviacOperatorType::Divide:		Result = (Right != 0.0) ? Left / Right : 0.0; break;
	default: break;
	}

	Agent->SetPropertyValue(ComputeNode->ResultProperty, FString::SanitizeFloat(Result));
	return EBehaviacStatus::Success;
}

// ===================================================================
// NOOP
// ===================================================================

UBehaviacBehaviorTask* UBehaviacNoop::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacNoopTask>(Outer);
}

EBehaviacStatus UBehaviacNoopTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	return EBehaviacStatus::Success;
}

// ===================================================================
// END
// ===================================================================

UBehaviacBehaviorTask* UBehaviacEnd::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacEndTask>(Outer);
}

void UBehaviacEnd::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	EndStatus = EBehaviacStatus::Success;
	bEndOutermost = false;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("EndStatus"))
		{
			if (Prop.Value == TEXT("BT_SUCCESS")) EndStatus = EBehaviacStatus::Success;
			else if (Prop.Value == TEXT("BT_FAILURE")) EndStatus = EBehaviacStatus::Failure;
		}
		else if (Prop.Name == TEXT("EndOutermost"))
		{
			bEndOutermost = (Prop.Value == TEXT("true"));
		}
	}
}

EBehaviacStatus UBehaviacEndTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacEnd* EndNode = Cast<UBehaviacEnd>(Node);
	return EndNode ? EndNode->EndStatus : EBehaviacStatus::Success;
}

// ===================================================================
// WAIT
// ===================================================================

UBehaviacBehaviorTask* UBehaviacWait::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacWaitTask>(Outer);
}

void UBehaviacWait::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Duration = 1.0f;

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Time"))
		{
			Duration = FCString::Atof(*Prop.Value);
		}
	}
}

UBehaviacWaitTask::UBehaviacWaitTask()
	: StartTime(0.0)
	, WaitDuration(0.0f)
{
}

bool UBehaviacWaitTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	const UBehaviacWait* WaitNode = Cast<UBehaviacWait>(Node);
	WaitDuration = WaitNode ? WaitNode->Duration : 1.0f;

	if (UWorld* World = Agent ? Agent->GetWorld() : nullptr)
	{
		StartTime = World->GetTimeSeconds();
	}
	else
	{
		StartTime = FPlatformTime::Seconds();
	}

	BEHAVIAC_VLOG(TEXT("[Wait] ENTER — duration=%.2fs, startTime=%.3f"), WaitDuration, StartTime);
	return true;
}

EBehaviacStatus UBehaviacWaitTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
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

	const double Elapsed = CurrentTime - StartTime;
	if (Elapsed >= WaitDuration)
	{
		BEHAVIAC_VLOG(TEXT("[Wait] COMPLETE — elapsed=%.3fs / %.2fs"), Elapsed, WaitDuration);
		return EBehaviacStatus::Success;
	}

	BEHAVIAC_VLOG(TEXT("[Wait] running — elapsed=%.3fs / %.2fs"), Elapsed, WaitDuration);
	return EBehaviacStatus::Running;
}

// ===================================================================
// WAIT FRAMES
// ===================================================================

UBehaviacBehaviorTask* UBehaviacWaitFrames::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacWaitFramesTask>(Outer);
}

void UBehaviacWaitFrames::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
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

UBehaviacWaitFramesTask::UBehaviacWaitFramesTask()
	: StartFrame(0)
	, TargetFrames(0)
{
}

bool UBehaviacWaitFramesTask::OnEnter(UBehaviacAgentComponent* Agent)
{
	const UBehaviacWaitFrames* WFNode = Cast<UBehaviacWaitFrames>(Node);
	TargetFrames = WFNode ? WFNode->FrameCount : 1;
	StartFrame = GFrameCounter;
	return true;
}

EBehaviacStatus UBehaviacWaitFramesTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	int32 Elapsed = static_cast<int32>(GFrameCounter - StartFrame);
	if (Elapsed >= TargetFrames)
	{
		return EBehaviacStatus::Success;
	}

	return EBehaviacStatus::Running;
}

// ===================================================================
// WAIT FOR SIGNAL
// ===================================================================

UBehaviacBehaviorTask* UBehaviacWaitForSignal::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacWaitForSignalTask>(Outer);
}

void UBehaviacWaitForSignal::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Signal"))
		{
			SignalName = Prop.Value;
		}
	}
}

EBehaviacStatus UBehaviacWaitForSignalTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacWaitForSignal* WFSNode = Cast<UBehaviacWaitForSignal>(Node);
	if (!WFSNode || !Agent)
	{
		return EBehaviacStatus::Failure;
	}

	if (Agent->IsSignalSet(WFSNode->SignalName))
	{
		return EBehaviacStatus::Success;
	}

	return EBehaviacStatus::Running;
}
