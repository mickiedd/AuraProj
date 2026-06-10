// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/BehaviacBehaviorTask.h"
#include "BehaviacConditions.generated.h"

class UBehaviacAgentComponent;

// ===================================================================
// CONDITION (base)
// ===================================================================

/**
 * Condition: evaluates a comparison between two values.
 */
UCLASS(DisplayName = "Condition")
class BEHAVIACRUNTIME_API UBehaviacCondition : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Condition")
	FString LeftOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Condition")
	FString RightOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Condition")
	EBehaviacOperatorType Operator;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacConditionTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

	/** Evaluate a comparison between two string values using the given operator */
	bool EvaluateComparison(const FString& Left, const FString& Right, EBehaviacOperatorType Op) const;
};

// ===================================================================
// AND
// ===================================================================

/**
 * And: logical AND of all children. All must succeed for this to succeed.
 * (Functionally same as Sequence but semantically used as a condition combiner.)
 */
UCLASS(DisplayName = "And")
class BEHAVIACRUNTIME_API UBehaviacAnd : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacAndTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// OR
// ===================================================================

/**
 * Or: logical OR of all children. Any succeeding is enough.
 * (Functionally same as Selector but semantically used as a condition combiner.)
 */
UCLASS(DisplayName = "Or")
class BEHAVIACRUNTIME_API UBehaviacOr : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacOrTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// TRUE
// ===================================================================

/**
 * True: always returns Success.
 */
UCLASS(DisplayName = "True")
class BEHAVIACRUNTIME_API UBehaviacTrue : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacTrueTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// FALSE
// ===================================================================

/**
 * False: always returns Failure.
 */
UCLASS(DisplayName = "False")
class BEHAVIACRUNTIME_API UBehaviacFalse : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacFalseTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};
