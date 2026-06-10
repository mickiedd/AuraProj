// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Conditions/BehaviacConditions.h"
#include "BehaviacAgent.h"

// ===================================================================
// CONDITION
// ===================================================================

UBehaviacBehaviorTask* UBehaviacCondition::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacConditionTask>(Outer);
}

void UBehaviacCondition::LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties)
{
	Super::LoadFromProperties(Version, InAgentType, Properties);
	Operator = EBehaviacOperatorType::Equal;

	for (const FBehaviacProperty& Prop : Properties)
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
			if (Prop.Value == TEXT("Equal")) Operator = EBehaviacOperatorType::Equal;
			else if (Prop.Value == TEXT("NotEqual")) Operator = EBehaviacOperatorType::NotEqual;
			else if (Prop.Value == TEXT("Greater")) Operator = EBehaviacOperatorType::Greater;
			else if (Prop.Value == TEXT("Less")) Operator = EBehaviacOperatorType::Less;
			else if (Prop.Value == TEXT("GreaterEqual")) Operator = EBehaviacOperatorType::GreaterEqual;
			else if (Prop.Value == TEXT("LessEqual")) Operator = EBehaviacOperatorType::LessEqual;
		}
	}

	// 加载期检测：Behaviac 中方法调用以 "()" 结尾，Condition 节点不允许使用方法作为操作数。
	// 在消息队列模式下，Condition 必须同步返回结果，方法调用已被移至异步队列，两者不兼容。
	if (LeftOperand.EndsWith(TEXT("()")))
	{
		UE_LOG(LogBehaviac, Error,
			TEXT("[Condition] 加载失败：LeftOperand='%s' 是方法调用。"
				 "消息队列模式下 Condition 节点不支持方法操作数，请改用属性（黑板变量）。"
				 "AgentType='%s'"),
			*LeftOperand, *InAgentType);
	}
	if (RightOperand.EndsWith(TEXT("()")))
	{
		UE_LOG(LogBehaviac, Error,
			TEXT("[Condition] 加载失败：RightOperand='%s' 是方法调用。"
				 "消息队列模式下 Condition 节点不支持方法操作数，请改用属性（黑板变量）。"
				 "AgentType='%s'"),
			*RightOperand, *InAgentType);
	}
}

EBehaviacStatus UBehaviacConditionTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	const UBehaviacCondition* CondNode = Cast<UBehaviacCondition>(Node);
	if (!CondNode || !Agent)
	{
		return EBehaviacStatus::Failure;
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
			UE_LOG(LogBehaviac, Warning,
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
			UE_LOG(LogBehaviac, Warning,
				TEXT("[Condition] RightOperand='%s' 对应的属性不存在或为空，"
					 "将以空字符串参与比较，结果可能不符合预期。"),
				*CondNode->RightOperand);
		}
	}

	return EvaluateComparison(LeftStr, RightStr, CondNode->Operator) ?
		EBehaviacStatus::Success : EBehaviacStatus::Failure;
}

bool UBehaviacConditionTask::EvaluateComparison(const FString& Left, const FString& Right, EBehaviacOperatorType Op) const
{
	// FVector 比较：检测 "X= Y= Z=" 格式
	// Greater/Less 系列按向量长度（SizeSquared）比较，适用于距离判断
	if (BehaviacIsVectorString(Left) && BehaviacIsVectorString(Right))
	{
		const FVector LeftVec  = BehaviacStringToVector(Left);
		const FVector RightVec = BehaviacStringToVector(Right);

		switch (Op)
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

	// Try numeric comparison
	if (Left.IsNumeric() && Right.IsNumeric())
	{
		double LeftVal = FCString::Atod(*Left);
		double RightVal = FCString::Atod(*Right);

		switch (Op)
		{
		case EBehaviacOperatorType::Equal:			return FMath::IsNearlyEqual(LeftVal, RightVal);
		case EBehaviacOperatorType::NotEqual:		return !FMath::IsNearlyEqual(LeftVal, RightVal);
		case EBehaviacOperatorType::Greater:			return LeftVal > RightVal;
		case EBehaviacOperatorType::Less:			return LeftVal < RightVal;
		case EBehaviacOperatorType::GreaterEqual:	return LeftVal >= RightVal;
		case EBehaviacOperatorType::LessEqual:		return LeftVal <= RightVal;
		default: return false;
		}
	}

	// String comparison
	int32 Cmp = Left.Compare(Right);
	switch (Op)
	{
	case EBehaviacOperatorType::Equal:			return Cmp == 0;
	case EBehaviacOperatorType::NotEqual:		return Cmp != 0;
	case EBehaviacOperatorType::Greater:			return Cmp > 0;
	case EBehaviacOperatorType::Less:			return Cmp < 0;
	case EBehaviacOperatorType::GreaterEqual:	return Cmp >= 0;
	case EBehaviacOperatorType::LessEqual:		return Cmp <= 0;
	default: return false;
	}
}

// ===================================================================
// AND
// ===================================================================

UBehaviacBehaviorTask* UBehaviacAnd::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacAndTask>(Outer);
}

EBehaviacStatus UBehaviacAndTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	for (UBehaviacBehaviorTask* Child : ChildTasks)
	{
		if (!Child) continue;

		EBehaviacStatus Result = Child->Execute(Agent, EBehaviacStatus::Invalid);
		if (Result != EBehaviacStatus::Success)
		{
			return Result; // Failure or Running propagates
		}
	}
	return EBehaviacStatus::Success;
}

// ===================================================================
// OR
// ===================================================================

UBehaviacBehaviorTask* UBehaviacOr::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacOrTask>(Outer);
}

EBehaviacStatus UBehaviacOrTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	for (UBehaviacBehaviorTask* Child : ChildTasks)
	{
		if (!Child) continue;

		EBehaviacStatus Result = Child->Execute(Agent, EBehaviacStatus::Invalid);
		if (Result == EBehaviacStatus::Success)
		{
			return EBehaviacStatus::Success;
		}
		if (Result == EBehaviacStatus::Running)
		{
			return EBehaviacStatus::Running;
		}
	}
	return EBehaviacStatus::Failure;
}

// ===================================================================
// TRUE / FALSE
// ===================================================================

UBehaviacBehaviorTask* UBehaviacTrue::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacTrueTask>(Outer);
}

EBehaviacStatus UBehaviacTrueTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	return EBehaviacStatus::Success;
}

UBehaviacBehaviorTask* UBehaviacFalse::CreateTask(UObject* Outer) const
{
	return NewObject<UBehaviacFalseTask>(Outer);
}

EBehaviacStatus UBehaviacFalseTask::OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus)
{
	return EBehaviacStatus::Failure;
}
