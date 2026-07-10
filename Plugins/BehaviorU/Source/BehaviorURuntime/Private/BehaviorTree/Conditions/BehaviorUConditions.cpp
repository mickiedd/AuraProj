// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Conditions/BehaviorUConditions.h"
#include "BehaviorUAgent.h"

// ===================================================================
// CONDITION
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUCondition::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUConditionTask>(Outer);
}

void UBehaviorUCondition::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Operator = EBehaviorUOperatorType::Equal;

	for (const FBehaviorUProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Opl"))
		{
			LeftOperand = Prop.Value;
		}
		else if (Prop.Name == TEXT("Opr"))
		{
			RightOperand = Prop.Value;
		}
		else if (Prop.Name == TEXT("Operator"))
		{
			if (Prop.Value == TEXT("Equal")) Operator = EBehaviorUOperatorType::Equal;
			else if (Prop.Value == TEXT("NotEqual")) Operator = EBehaviorUOperatorType::NotEqual;
			else if (Prop.Value == TEXT("Greater")) Operator = EBehaviorUOperatorType::Greater;
			else if (Prop.Value == TEXT("Less")) Operator = EBehaviorUOperatorType::Less;
			else if (Prop.Value == TEXT("GreaterEqual")) Operator = EBehaviorUOperatorType::GreaterEqual;
			else if (Prop.Value == TEXT("LessEqual")) Operator = EBehaviorUOperatorType::LessEqual;
		}
	}

	// 加载期检测：BehaviorU 中方法调用以 "()" 结尾，Condition 节点不允许使用方法作为操作数。
	// 在消息队列模式下，Condition 必须同步返回结果，方法调用已被移至异步队列，两者不兼容。
	if (LeftOperand.EndsWith(TEXT("()")))
	{
		UE_LOG(LogBehaviorU, Error,
			TEXT("[Condition] 加载失败：LeftOperand='%s' 是方法调用。"
				 "消息队列模式下 Condition 节点不支持方法操作数，请改用属性（黑板变量）。"
				 "AgentType='%s'"),
			*LeftOperand, *InAgentType);
	}
	if (RightOperand.EndsWith(TEXT("()")))
	{
		UE_LOG(LogBehaviorU, Error,
			TEXT("[Condition] 加载失败：RightOperand='%s' 是方法调用。"
				 "消息队列模式下 Condition 节点不支持方法操作数，请改用属性（黑板变量）。"
				 "AgentType='%s'"),
			*RightOperand, *InAgentType);
	}
}

EBehaviorUStatus UBehaviorUConditionTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	const UBehaviorUCondition* CondNode = Cast<UBehaviorUCondition>(Node);
	if (!CondNode || !Agent)
	{
		return EBehaviorUStatus::Failure;
	}

	FString LeftStr = CondNode->LeftOperand;
	FString RightStr = CondNode->RightOperand;

	// 解析左操作数（仅从黑板属性读取）
	if (LeftStr.StartsWith(TEXT("Self.")))
	{
		// 运行期检测：方法调用以 "()" 结尾，在消息队列模式下不允许
		ensureMsgf(!LeftStr.EndsWith(TEXT("()")),
			TEXT("[Condition] LeftOperand='%s' 是方法调用，消息队列模式下 Condition 节点"
				 "必须使用属性（黑板变量），不能使用方法。请修改行为树配置。"),
			*LeftStr);

		LeftStr = Agent->GetPropertyValue(LeftStr);

		// 属性未设置时发出警告，但继续评估（空字符串参与比较）
		if (LeftStr.IsEmpty())
		{
			UE_LOG(LogBehaviorU, Warning,
				TEXT("[Condition] LeftOperand='%s' 对应的属性不存在或为空，"
					 "将以空字符串参与比较，结果可能不符合预期。"),
				*CondNode->LeftOperand);
		}
	}

	// 解析右操作数（仅从黑板属性读取）
	if (RightStr.StartsWith(TEXT("Self.")))
	{
		ensureMsgf(!RightStr.EndsWith(TEXT("()")),
			TEXT("[Condition] RightOperand='%s' 是方法调用，消息队列模式下 Condition 节点"
				 "必须使用属性（黑板变量），不能使用方法。请修改行为树配置。"),
			*RightStr);

		RightStr = Agent->GetPropertyValue(RightStr);

		if (RightStr.IsEmpty())
		{
			UE_LOG(LogBehaviorU, Warning,
				TEXT("[Condition] RightOperand='%s' 对应的属性不存在或为空，"
					 "将以空字符串参与比较，结果可能不符合预期。"),
				*CondNode->RightOperand);
		}
	}

	return EvaluateComparison(LeftStr, RightStr, CondNode->Operator) ?
		EBehaviorUStatus::Success : EBehaviorUStatus::Failure;
}

bool UBehaviorUConditionTask::EvaluateComparison(const FString& Left, const FString& Right, EBehaviorUOperatorType Op) const
{
	// FVector 比较：检测 "X= Y= Z=" 格式
	// Greater/Less 系列按向量长度（SizeSquared）比较，适用于距离判断
	if (BehaviorUIsVectorString(Left) && BehaviorUIsVectorString(Right))
	{
		const FVector LeftVec  = BehaviorUStringToVector(Left);
		const FVector RightVec = BehaviorUStringToVector(Right);

		switch (Op)
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

	// Try numeric comparison
	if (Left.IsNumeric() && Right.IsNumeric())
	{
		double LeftVal = FCString::Atod(*Left);
		double RightVal = FCString::Atod(*Right);

		switch (Op)
		{
		case EBehaviorUOperatorType::Equal:			return FMath::IsNearlyEqual(LeftVal, RightVal);
		case EBehaviorUOperatorType::NotEqual:		return !FMath::IsNearlyEqual(LeftVal, RightVal);
		case EBehaviorUOperatorType::Greater:			return LeftVal > RightVal;
		case EBehaviorUOperatorType::Less:			return LeftVal < RightVal;
		case EBehaviorUOperatorType::GreaterEqual:	return LeftVal >= RightVal;
		case EBehaviorUOperatorType::LessEqual:		return LeftVal <= RightVal;
		default: return false;
		}
	}

	// String comparison
	int32 Cmp = Left.Compare(Right);
	switch (Op)
	{
	case EBehaviorUOperatorType::Equal:			return Cmp == 0;
	case EBehaviorUOperatorType::NotEqual:		return Cmp != 0;
	case EBehaviorUOperatorType::Greater:			return Cmp > 0;
	case EBehaviorUOperatorType::Less:			return Cmp < 0;
	case EBehaviorUOperatorType::GreaterEqual:	return Cmp >= 0;
	case EBehaviorUOperatorType::LessEqual:		return Cmp <= 0;
	default: return false;
	}
}

// ===================================================================
// AND
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUAnd::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUAndTask>(Outer);
}

EBehaviorUStatus UBehaviorUAndTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	for (UBehaviorUBehaviorTask* Child : ChildTasks)
	{
		if (!Child) continue;

		EBehaviorUStatus Result = Child->Execute(Agent, EBehaviorUStatus::Invalid);
		if (Result != EBehaviorUStatus::Success)
		{
			return Result; // Failure or Running propagates
		}
	}
	return EBehaviorUStatus::Success;
}

// ===================================================================
// OR
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUOr::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUOrTask>(Outer);
}

EBehaviorUStatus UBehaviorUOrTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	for (UBehaviorUBehaviorTask* Child : ChildTasks)
	{
		if (!Child) continue;

		EBehaviorUStatus Result = Child->Execute(Agent, EBehaviorUStatus::Invalid);
		if (Result == EBehaviorUStatus::Success)
		{
			return EBehaviorUStatus::Success;
		}
		if (Result == EBehaviorUStatus::Running)
		{
			return EBehaviorUStatus::Running;
		}
	}
	return EBehaviorUStatus::Failure;
}

// ===================================================================
// TRUE / FALSE
// ===================================================================

UBehaviorUBehaviorTask* UBehaviorUTrue::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUTrueTask>(Outer);
}

EBehaviorUStatus UBehaviorUTrueTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	return EBehaviorUStatus::Success;
}

UBehaviorUBehaviorTask* UBehaviorUFalse::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviorUFalseTask>(Outer);
}

EBehaviorUStatus UBehaviorUFalseTask::OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus)
{
	return EBehaviorUStatus::Failure;
}
