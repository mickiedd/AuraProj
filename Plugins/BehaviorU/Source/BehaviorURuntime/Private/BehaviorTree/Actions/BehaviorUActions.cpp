// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Actions/BehaviorUActions.h"
#include "BehaviorUAgent.h"

// ===================================================================
// ACTION
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUAction::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUActionTask>(Outer);
}

void UBehaviorUAction::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	ResultOption = EBehaviorUStatus::Success;

	BEHAVIORU_VLOG(TEXT("[BehaviorU] Action::LoadFromProperties: Got %d properties"), Properties.Num());

	for (const FBehaviorUProperty& Prop : Properties)
	{
		BEHAVIORU_VLOG(TEXT("[BehaviorU]    Property: '%s' = '%s'"), *Prop.Name, *Prop.Value);

		if (Prop.Name == TEXT("Method"))
		{
			MethodName = Prop.Value;
			BEHAVIORU_VLOG(TEXT("[BehaviorU]    Set MethodName to '%s'"), *MethodName);
		}
		else if (Prop.Name == TEXT("ResultOption"))
		{
			if (Prop.Value == TEXT("BT_SUCCESS")) ResultOption = EBehaviorUStatus::Success;
			else if (Prop.Value == TEXT("BT_FAILURE")) ResultOption = EBehaviorUStatus::Failure;
			else if (Prop.Value == TEXT("BT_RUNNING")) ResultOption = EBehaviorUStatus::Running;
		}
	}

	BEHAVIORU_VLOG(TEXT("[BehaviorU] After parsing: MethodName='%s'"), *MethodName);
}

EBehaviorUStatus UBehaviorUActionTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUAction* ActionNode = Cast<UBehaviorUAction>(Node);
	if (!ActionNode || !Agent)
	{
		BEHAVIORU_VLOG(TEXT("[BehaviorU] ActionTask::OnUpdate: ActionNode=%d, Agent=%d"),
			ActionNode != nullptr, Agent != nullptr);
		return EBehaviorUStatus::Failure;
	}

	// --- 第一次调用：提交命令到队列 ---
	if (!bHasPendingCommand)
	{
		// ResultOption != Running 表示设计者要覆盖最终结果，方法调用为即发即忘
		const bool bNeedResult = (ActionNode->ResultOption == EBehaviorUStatus::Running);

		PendingCommandId = Agent->EnqueueMethodCommand(ActionNode->MethodName, bNeedResult);

		BEHAVIORU_VLOG(TEXT("[BehaviorU] ActionTask::OnUpdate: 已入队 id=%llu method='%s' needResult=%d"),
			PendingCommandId, *ActionNode->MethodName, (int32)bNeedResult);

		if (!bNeedResult)
		{
			// 即发即忘：方法副作用会在下帧执行，本节点直接返回设计者指定的状态
			PendingCommandId = 0;
			return ActionNode->ResultOption;
		}

		bHasPendingCommand = true;
		return EBehaviorUStatus::Running;
	}

	// --- 后续调用：等待上帧命令的执行结果 ---
	EBehaviorUStatus Result;
	if (Agent->GetCommandResult(PendingCommandId, Result))
	{
		BEHAVIORU_VLOG(TEXT("[BehaviorU] ActionTask::OnUpdate: id=%llu method='%s' 结果=%d"),
			PendingCommandId, *ActionNode->MethodName, (int32)Result);

		bHasPendingCommand = false;
		PendingCommandId   = 0;

		// Running 优先（方法自身请求继续异步执行）
		if (Result == EBehaviorUStatus::Running)
		{
			return EBehaviorUStatus::Running;
		}

		// ResultOption == Running means "forward the method's real result."
		// If the method returned Invalid (no handler registered), treat it as
		// Failure so missing handlers are visible instead of silently succeeding.
		if (ActionNode->ResultOption == EBehaviorUStatus::Running)
		{
			return (Result != EBehaviorUStatus::Invalid) ? Result : EBehaviorUStatus::Failure;
		}

		return ActionNode->ResultOption;
	}

	// 结果尚未就绪，继续等待
	return EBehaviorUStatus::Running;
}

void UBehaviorUActionTask::OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
{
	// 若树被外部中断（Selector 子节点失败导致退出等情况），清理残留结果条目
	if (bHasPendingCommand && Agent)
	{
		BEHAVIORU_VLOG(TEXT("[BehaviorU] ActionTask::OnExit: 中断清理 id=%llu"), PendingCommandId);
		Agent->ClearCommandResult(PendingCommandId);
		bHasPendingCommand = false;
		PendingCommandId   = 0;
	}

	Super::OnExit(Agent, InStatus);
}

void UBehaviorUActionTask::Reset(UBehaviorUAgentComponent* Agent)
{
	// ResetBehaviorTree() 不经过 OnExit，需在此主动清理挂起命令状态
	if (bHasPendingCommand && Agent)
	{
		UE_LOG(LogBehaviorU, Verbose, TEXT("[BehaviorU] ActionTask::Reset: 清理残留挂起命令 id=%llu"), PendingCommandId);
		Agent->ClearCommandResult(PendingCommandId);
	}
	bHasPendingCommand = false;
	PendingCommandId   = 0;

	Super::Reset(Agent);
}

// ===================================================================
// ASSIGNMENT
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUAssignment::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUAssignmentTask>(Outer);
}

void UBehaviorUAssignment::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	bCastFromRight = false;

	for (const FBehaviorUProperty& Prop : Properties)
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

EBehaviorUStatus UBehaviorUAssignmentTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUAssignment* AssignNode = Cast<UBehaviorUAssignment>(Node);
	if (!AssignNode || !Agent)
	{
		return EBehaviorUStatus::Failure;
	}

	FString Value = AssignNode->PropertyValue;

	// Resolve if it references another property
	if (Value.StartsWith(TEXT("Self.")))
	{
		Value = Agent->GetPropertyValue(Value);
	}

	Agent->SetPropertyValue(AssignNode->PropertyName, Value);
	return EBehaviorUStatus::Success;
}

// ===================================================================
// COMPUTE
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUCompute::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUComputeTask>(Outer);
}

void UBehaviorUCompute::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Operator = EBehaviorUOperatorType::Add;

	for (const FBehaviorUProperty& Prop : Properties)
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
			if (Prop.Value == TEXT("Add")) Operator = EBehaviorUOperatorType::Add;
			else if (Prop.Value == TEXT("Sub")) Operator = EBehaviorUOperatorType::Subtract;
			else if (Prop.Value == TEXT("Mul")) Operator = EBehaviorUOperatorType::Multiply;
			else if (Prop.Value == TEXT("Div")) Operator = EBehaviorUOperatorType::Divide;
		}
	}
}

EBehaviorUStatus UBehaviorUComputeTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUCompute* ComputeNode = Cast<UBehaviorUCompute>(Node);
	if (!ComputeNode || !Agent)
	{
		return EBehaviorUStatus::Failure;
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
	if (BehaviorUIsVectorString(LeftStr))
	{
		const FVector LeftVec = BehaviorUStringToVector(LeftStr);
		FVector VecResult = FVector::ZeroVector;

		if (BehaviorUIsVectorString(RightStr))
		{
			// 两侧均为向量：逐分量运算
			const FVector RightVec = BehaviorUStringToVector(RightStr);
			switch (ComputeNode->Operator)
			{
			case EBehaviorUOperatorType::Add:
				VecResult = LeftVec + RightVec; break;
			case EBehaviorUOperatorType::Subtract:
				VecResult = LeftVec - RightVec; break;
			case EBehaviorUOperatorType::Multiply:
				VecResult = LeftVec * RightVec; break;
			case EBehaviorUOperatorType::Divide:
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
			case EBehaviorUOperatorType::Add:
				VecResult = LeftVec + FVector(Scalar); break;
			case EBehaviorUOperatorType::Subtract:
				VecResult = LeftVec - FVector(Scalar); break;
			case EBehaviorUOperatorType::Multiply:
				VecResult = LeftVec * Scalar; break;
			case EBehaviorUOperatorType::Divide:
				VecResult = (Scalar != 0.f) ? LeftVec / Scalar : FVector::ZeroVector; break;
			default: break;
			}
		}

		Agent->SetPropertyValue(ComputeNode->ResultProperty, BehaviorUVectorToString(VecResult));
		return EBehaviorUStatus::Success;
	}

	double Left = FCString::Atod(*LeftStr);
	double Right = FCString::Atod(*RightStr);
	double Result = 0.0;

	switch (ComputeNode->Operator)
	{
	case EBehaviorUOperatorType::Add:		Result = Left + Right; break;
	case EBehaviorUOperatorType::Subtract:	Result = Left - Right; break;
	case EBehaviorUOperatorType::Multiply:	Result = Left * Right; break;
	case EBehaviorUOperatorType::Divide:		Result = (Right != 0.0) ? Left / Right : 0.0; break;
	default: break;
	}

	Agent->SetPropertyValue(ComputeNode->ResultProperty, FString::SanitizeFloat(Result));
	return EBehaviorUStatus::Success;
}

// ===================================================================
// NOOP
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUNoop::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUNoopTask>(Outer);
}

EBehaviorUStatus UBehaviorUNoopTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	return EBehaviorUStatus::Success;
}

// ===================================================================
// END
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUEnd::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUEndTask>(Outer);
}

void UBehaviorUEnd::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	EndStatus = EBehaviorUStatus::Success;
	bEndOutermost = false;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("EndStatus"))
		{
			if (Prop.Value == TEXT("BT_SUCCESS")) EndStatus = EBehaviorUStatus::Success;
			else if (Prop.Value == TEXT("BT_FAILURE")) EndStatus = EBehaviorUStatus::Failure;
		}
		else if (Prop.Name == TEXT("EndOutermost"))
		{
			bEndOutermost = (Prop.Value == TEXT("true"));
		}
	}
}

EBehaviorUStatus UBehaviorUEndTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUEnd* EndNode = Cast<UBehaviorUEnd>(Node);
	return EndNode ? EndNode->EndStatus : EBehaviorUStatus::Success;
}

// ===================================================================
// WAIT
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUWait::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUWaitTask>(Outer);
}

void UBehaviorUWait::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Duration = 1.0f;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Time"))
		{
			Duration = FCString::Atof(*Prop.Value);
		}
	}
}

UBehaviorUWaitTask::UBehaviorUWaitTask()
	: StartTime(0.0)
	, WaitDuration(0.0f)
{
}

bool UBehaviorUWaitTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UBehaviorUWait* WaitNode = Cast<UBehaviorUWait>(Node);
	WaitDuration = WaitNode ? WaitNode->Duration : 1.0f;

	if (UWorld* World = Agent ? Agent->GetWorld() : nullptr)
	{
		StartTime = World->GetTimeSeconds();
	}
	else
	{
		StartTime = FPlatformTime::Seconds();
	}

	BEHAVIORU_VLOG(TEXT("[Wait] ENTER — duration=%.2fs, startTime=%.3f"), WaitDuration, StartTime);
	return true;
}

EBehaviorUStatus UBehaviorUWaitTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
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
		BEHAVIORU_VLOG(TEXT("[Wait] COMPLETE — elapsed=%.3fs / %.2fs"), Elapsed, WaitDuration);
		return EBehaviorUStatus::Success;
	}

	BEHAVIORU_VLOG(TEXT("[Wait] running — elapsed=%.3fs / %.2fs"), Elapsed, WaitDuration);
	return EBehaviorUStatus::Running;
}

// ===================================================================
// WAIT FRAMES
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUWaitFrames::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUWaitFramesTask>(Outer);
}

void UBehaviorUWaitFrames::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
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

UBehaviorUWaitFramesTask::UBehaviorUWaitFramesTask()
	: StartFrame(0)
	, TargetFrames(0)
{
}

bool UBehaviorUWaitFramesTask::OnEnter(UBehaviorUAgentComponent* Agent)
{
	const UBehaviorUWaitFrames* WFNode = Cast<UBehaviorUWaitFrames>(Node);
	TargetFrames = WFNode ? WFNode->FrameCount : 1;
	StartFrame = GFrameCounter;
	return true;
}

EBehaviorUStatus UBehaviorUWaitFramesTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	int32 Elapsed = static_cast<int32>(GFrameCounter - StartFrame);
	if (Elapsed >= TargetFrames)
	{
		return EBehaviorUStatus::Success;
	}

	return EBehaviorUStatus::Running;
}

// ===================================================================
// WAIT FOR SIGNAL
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUWaitForSignal::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUWaitForSignalTask>(Outer);
}

void UBehaviorUWaitForSignal::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Signal"))
		{
			SignalName = Prop.Value;
		}
	}
}

EBehaviorUStatus UBehaviorUWaitForSignalTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUWaitForSignal* WFSNode = Cast<UBehaviorUWaitForSignal>(Node);
	if (!WFSNode || !Agent)
	{
		return EBehaviorUStatus::Failure;
	}

	if (Agent->IsSignalSet(WFSNode->SignalName))
	{
		return EBehaviorUStatus::Success;
	}

	return EBehaviorUStatus::Running;
}
