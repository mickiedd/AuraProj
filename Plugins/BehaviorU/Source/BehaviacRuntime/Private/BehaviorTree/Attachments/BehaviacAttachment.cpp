// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#include "BehaviorTree/Attachments/BehaviacAttachment.h"
#include "BehaviacAgent.h"

// ===================================================================
// UBehaviacAttachment
// ===================================================================

UBehaviacAttachment::UBehaviacAttachment()
	: PreconditionPhase(EBehaviacPreconditionPhase::Both)
	, EffectorPhase(EBehaviacEffectorPhase::Both)
	, ActionResult(EBehaviacActionResult::All)
	, bNegate(false)
{
}

void UBehaviacAttachment::LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviacProperty>& Properties)
{
	for (const FBehaviacProperty& Prop : Properties)
	{
		if (Prop.Name == TEXT("Phase"))
		{
			if (Prop.Value == TEXT("Enter"))
				PreconditionPhase = EBehaviacPreconditionPhase::Enter;
			else if (Prop.Value == TEXT("Update"))
				PreconditionPhase = EBehaviacPreconditionPhase::Update;
			else
				PreconditionPhase = EBehaviacPreconditionPhase::Both;
		}
		else if (Prop.Name == TEXT("Negate"))
		{
			bNegate = (Prop.Value == TEXT("true"));
		}
	}
}

bool UBehaviacAttachment::Evaluate(UBehaviacAgentComponent* Agent) const
{
	return true;
}

void UBehaviacAttachment::Apply(UBehaviacAgentComponent* Agent, bool bSuccess) const
{
}

bool UBehaviacAttachment::AppliesToPhase(EBehaviacPreconditionPhase Phase) const
{
	return PreconditionPhase == EBehaviacPreconditionPhase::Both || PreconditionPhase == Phase;
}

// ===================================================================
// UBehaviacPrecondition
// ===================================================================

UBehaviacPrecondition::UBehaviacPrecondition()
	: Operator(EBehaviacOperatorType::Equal)
{
}

void UBehaviacPrecondition::LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviacProperty>& Properties)
{
	// Load base attachment properties (Phase, Negate)
	Super::LoadFromProperties(Version, AgentType, Properties);

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
			if (Prop.Value == TEXT("Equal"))				Operator = EBehaviacOperatorType::Equal;
			else if (Prop.Value == TEXT("NotEqual"))		Operator = EBehaviacOperatorType::NotEqual;
			else if (Prop.Value == TEXT("Greater"))		Operator = EBehaviacOperatorType::Greater;
			else if (Prop.Value == TEXT("Less"))			Operator = EBehaviacOperatorType::Less;
			else if (Prop.Value == TEXT("GreaterEqual"))	Operator = EBehaviacOperatorType::GreaterEqual;
			else if (Prop.Value == TEXT("LessEqual"))		Operator = EBehaviacOperatorType::LessEqual;
		}
	}
}

bool UBehaviacPrecondition::AppliesToPhase(EBehaviacPreconditionPhase Phase) const
{
	return PreconditionPhase == EBehaviacPreconditionPhase::Both || PreconditionPhase == Phase;
}

bool UBehaviacPrecondition::Evaluate(UBehaviacAgentComponent* Agent) const
{
	if (!Agent)
	{
		return false;
	}

	// Get left and right values from agent properties
	FString LeftValue = Agent->GetPropertyValue(LeftOperand);
	FString RightValue = RightOperand;

	// If right operand references a property, resolve it
	if (RightOperand.StartsWith(TEXT("Self.")))
	{
		RightValue = Agent->GetPropertyValue(RightOperand);
	}

	bool bResult = false;

	// FVector 比较：检测 "X= Y= Z=" 格式，Greater/Less 系列按向量长度比较
	if (BehaviacIsVectorString(LeftValue) && BehaviacIsVectorString(RightValue))
	{
		const FVector LeftVec  = BehaviacStringToVector(LeftValue);
		const FVector RightVec = BehaviacStringToVector(RightValue);

		switch (Operator)
		{
		case EBehaviacOperatorType::Equal:			bResult = LeftVec.Equals(RightVec); break;
		case EBehaviacOperatorType::NotEqual:		bResult = !LeftVec.Equals(RightVec); break;
		case EBehaviacOperatorType::Greater:		bResult = LeftVec.SizeSquared() > RightVec.SizeSquared(); break;
		case EBehaviacOperatorType::Less:			bResult = LeftVec.SizeSquared() < RightVec.SizeSquared(); break;
		case EBehaviacOperatorType::GreaterEqual:	bResult = LeftVec.SizeSquared() >= RightVec.SizeSquared(); break;
		case EBehaviacOperatorType::LessEqual:		bResult = LeftVec.SizeSquared() <= RightVec.SizeSquared(); break;
		default: break;
		}

		return bNegate ? !bResult : bResult;
	}

	// Try numeric comparison first
	bool bIsNumeric = LeftValue.IsNumeric() && RightValue.IsNumeric();

	if (bIsNumeric)
	{
		double Left = FCString::Atod(*LeftValue);
		double Right = FCString::Atod(*RightValue);

		switch (Operator)
		{
		case EBehaviacOperatorType::Equal:			bResult = FMath::IsNearlyEqual(Left, Right); break;
		case EBehaviacOperatorType::NotEqual:		bResult = !FMath::IsNearlyEqual(Left, Right); break;
		case EBehaviacOperatorType::Greater:			bResult = Left > Right; break;
		case EBehaviacOperatorType::Less:			bResult = Left < Right; break;
		case EBehaviacOperatorType::GreaterEqual:	bResult = Left >= Right; break;
		case EBehaviacOperatorType::LessEqual:		bResult = Left <= Right; break;
		default: break;
		}
	}
	else
	{
		// String comparison
		int32 Cmp = LeftValue.Compare(RightValue);
		switch (Operator)
		{
		case EBehaviacOperatorType::Equal:			bResult = (Cmp == 0); break;
		case EBehaviacOperatorType::NotEqual:		bResult = (Cmp != 0); break;
		case EBehaviacOperatorType::Greater:			bResult = (Cmp > 0); break;
		case EBehaviacOperatorType::Less:			bResult = (Cmp < 0); break;
		case EBehaviacOperatorType::GreaterEqual:	bResult = (Cmp >= 0); break;
		case EBehaviacOperatorType::LessEqual:		bResult = (Cmp <= 0); break;
		default: break;
		}
	}

	return bNegate ? !bResult : bResult;
}

// ===================================================================
// UBehaviacEffector
// ===================================================================

UBehaviacEffector::UBehaviacEffector()
{
}

void UBehaviacEffector::LoadFromProperties(int32 Version, const FString& AgentType, const TArray<FBehaviacProperty>& Properties)
{
	// Load base attachment properties (Phase, Negate).
	Super::LoadFromProperties(Version, AgentType, Properties);

	// Default to "apply on any completion" if no phase is specified.
	EffectorPhase = EBehaviacEffectorPhase::Both;

	for (const FBehaviacProperty& Prop : Properties)
	{
		// Accept both "PropertyName"/"Opl" and "PropertyValue"/"Opr" naming conventions
		// so effectors exported by the behaviac editor (Opl/Opr) and hand-authored ones
		// (PropertyName/PropertyValue) both work.
		if (Prop.Name == TEXT("PropertyName") || Prop.Name == TEXT("Opl"))
		{
			PropertyName = Prop.Value;
		}
		else if (Prop.Name == TEXT("PropertyValue") || Prop.Name == TEXT("Opr"))
		{
			PropertyValue = Prop.Value;
		}
		else if (Prop.Name == TEXT("Phase"))
		{
			if (Prop.Value == TEXT("Success"))			EffectorPhase = EBehaviacEffectorPhase::Success;
			else if (Prop.Value == TEXT("Failure"))	EffectorPhase = EBehaviacEffectorPhase::Failure;
			else										EffectorPhase = EBehaviacEffectorPhase::Both;
		}
	}
}

void UBehaviacEffector::Apply(UBehaviacAgentComponent* Agent, bool bSuccess) const
{
	if (!Agent)
	{
		return;
	}

	bool bShouldApply = false;

	switch (EffectorPhase)
	{
	case EBehaviacEffectorPhase::Success:	bShouldApply = bSuccess; break;
	case EBehaviacEffectorPhase::Failure:	bShouldApply = !bSuccess; break;
	case EBehaviacEffectorPhase::Both:		bShouldApply = true; break;
	}

	if (bShouldApply && !PropertyName.IsEmpty())
	{
		Agent->SetPropertyValue(PropertyName, PropertyValue);
	}
}

// ===================================================================
// UBehaviacEventAttachment
// ===================================================================

UBehaviacEventAttachment::UBehaviacEventAttachment()
	: bTriggeredOnce(true)
{
}
