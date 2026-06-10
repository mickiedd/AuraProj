// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/BehaviacBehaviorTask.h"
#include "BehaviacComposites.generated.h"

class UBehaviacAgentComponent;

// ===================================================================
// SELECTOR
// ===================================================================

/**
 * Selector: ticks children left to right until one succeeds or returns Running.
 * Returns Success if any child succeeds, Failure if all fail.
 */
UCLASS(DisplayName = "Selector")
class BEHAVIACRUNTIME_API UBehaviacSelector : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;
	virtual bool Evaluate(UBehaviacAgentComponent* Agent) const;
	bool CheckIfInterrupted(UBehaviacAgentComponent* Agent) const;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacSelectorTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual void OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// SEQUENCE
// ===================================================================

/**
 * Sequence: ticks children left to right until one fails or returns Running.
 * Returns Failure if any child fails, Success if all succeed.
 */
UCLASS(DisplayName = "Sequence")
class BEHAVIACRUNTIME_API UBehaviacSequence : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacSequenceTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual void OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// PARALLEL
// ===================================================================

/**
 * Parallel: ticks all children simultaneously every frame.
 * Completion policy determines success/failure behavior.
 */
UCLASS(DisplayName = "Parallel")
class BEHAVIACRUNTIME_API UBehaviacParallel : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	UBehaviacParallel();

	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Parallel")
	EBehaviacParallelPolicy FailurePolicy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Parallel")
	EBehaviacParallelPolicy SuccessPolicy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Parallel")
	EBehaviacChildFinishPolicy ChildFinishPolicy;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacParallelTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviacParallelTask();

	virtual void Init(UBehaviacBehaviorNode* InNode) override;
	virtual void Reset(UBehaviacAgentComponent* Agent) override;

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	/** Status of each child in this parallel execution */
	TArray<EBehaviacStatus> ChildStatuses;
};

// ===================================================================
// IF-ELSE
// ===================================================================

/**
 * IfElse: expects 3 children: condition, if-true branch, if-false branch.
 */
UCLASS(DisplayName = "IfElse")
class BEHAVIACRUNTIME_API UBehaviacIfElse : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacIfElseTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// SELECTOR LOOP
// ===================================================================

/**
 * SelectorLoop: a selector that re-evaluates its conditions on each tick.
 * If a higher-priority child becomes valid, it interrupts the current child.
 */
UCLASS(DisplayName = "SelectorLoop")
class BEHAVIACRUNTIME_API UBehaviacSelectorLoop : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacSelectorLoopTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// SELECTOR PROBABILITY
// ===================================================================

/**
 * SelectorProbability: selects a child randomly based on weight values.
 */
UCLASS(DisplayName = "SelectorProbability")
class BEHAVIACRUNTIME_API UBehaviacSelectorProbability : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacSelectorProbabilityTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// SELECTOR STOCHASTIC
// ===================================================================

/**
 * SelectorStochastic: a selector that shuffles child order before evaluation.
 */
UCLASS(DisplayName = "SelectorStochastic")
class BEHAVIACRUNTIME_API UBehaviacSelectorStochastic : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacSelectorStochasticTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviacSelectorStochasticTask();

	virtual void Reset(UBehaviacAgentComponent* Agent) override;

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

	/** Randomized execution order */
	TArray<int32> ShuffledOrder;
};

// ===================================================================
// SEQUENCE STOCHASTIC
// ===================================================================

/**
 * SequenceStochastic: a sequence that shuffles child order before evaluation.
 */
UCLASS(DisplayName = "SequenceStochastic")
class BEHAVIACRUNTIME_API UBehaviacSequenceStochastic : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacSequenceStochasticTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviacSequenceStochasticTask();

	virtual void Reset(UBehaviacAgentComponent* Agent) override;

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

	TArray<int32> ShuffledOrder;
};

// ===================================================================
// REFERENCE BEHAVIOR (Sub-Tree)
// ===================================================================

/**
 * ReferenceBehavior: references another behavior tree for execution.
 */
UCLASS(DisplayName = "ReferenceBehavior")
class BEHAVIACRUNTIME_API UBehaviacReferenceBehavior : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** Path to the referenced behavior tree */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Reference")
	FString ReferencedTreePath;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacReferenceBehaviorTask : public UBehaviacSingleChildTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

	/** The loaded sub-tree task */
	UPROPERTY()
	UBehaviacBehaviorTreeTask* SubTreeTask;
};

// ===================================================================
// WITH PRECONDITION
// ===================================================================

/**
 * WithPrecondition: expects 2 children. The first is the precondition,
 * the second is the action. Only runs the action if precondition succeeds.
 */
UCLASS(DisplayName = "WithPrecondition")
class BEHAVIACRUNTIME_API UBehaviacWithPrecondition : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacWithPreconditionTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};
