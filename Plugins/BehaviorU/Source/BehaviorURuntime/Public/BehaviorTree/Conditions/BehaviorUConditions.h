// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorUConditions.generated.h"

class UBehaviorUAgentComponent;

// ===================================================================
// CONDITION (base)
// ===================================================================

/**
 * Condition: evaluates a comparison between two values.
 */
UCLASS(DisplayName = "Condition")
class BEHAVIORURUNTIME_API UBehaviorUCondition : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Condition")
	FString LeftOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Condition")
	FString RightOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Condition")
	EBehaviorUOperatorType Operator;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUConditionTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

	/** Evaluate a comparison between two string values using the given operator */
	bool EvaluateComparison(const FString& Left, const FString& Right, EBehaviorUOperatorType Op) const;
};

// ===================================================================
// AND
// ===================================================================

/**
 * And: logical AND of all children. All must succeed for this to succeed.
 * (Functionally same as Sequence but semantically used as a condition combiner.)
 */
UCLASS(DisplayName = "And")
class BEHAVIORURUNTIME_API UBehaviorUAnd : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUAndTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// OR
// ===================================================================

/**
 * Or: logical OR of all children. Any succeeding is enough.
 * (Functionally same as Selector but semantically used as a condition combiner.)
 */
UCLASS(DisplayName = "Or")
class BEHAVIORURUNTIME_API UBehaviorUOr : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUOrTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// TRUE
// ===================================================================

/**
 * True: always returns Success.
 */
UCLASS(DisplayName = "True")
class BEHAVIORURUNTIME_API UBehaviorUTrue : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUTrueTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// FALSE
// ===================================================================

/**
 * False: always returns Failure.
 */
UCLASS(DisplayName = "False")
class BEHAVIORURUNTIME_API UBehaviorUFalse : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUFalseTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};
