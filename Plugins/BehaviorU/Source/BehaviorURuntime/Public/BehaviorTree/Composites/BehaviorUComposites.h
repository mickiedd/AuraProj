// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorUComposites.generated.h"

class UBehaviorUAgentComponent;

// ===================================================================
// SELECTOR
// ===================================================================

/**
 * Selector: ticks children left to right until one succeeds or returns Running.
 * Returns Success if any child succeeds, Failure if all fail.
 */
UCLASS(DisplayName = "Selector")
class BEHAVIORURUNTIME_API UBehaviorUSelector : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;
	virtual bool Evaluate(UBehaviorUAgentComponent* Agent) const;
	bool CheckIfInterrupted(UBehaviorUAgentComponent* Agent) const;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUSelectorTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// SEQUENCE
// ===================================================================

/**
 * Sequence: ticks children left to right until one fails or returns Running.
 * Returns Failure if any child fails, Success if all succeed.
 */
UCLASS(DisplayName = "Sequence")
class BEHAVIORURUNTIME_API UBehaviorUSequence : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUSequenceTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// PARALLEL
// ===================================================================

/**
 * Parallel: ticks all children simultaneously every frame.
 * Completion policy determines success/failure behavior.
 */
UCLASS(DisplayName = "Parallel")
class BEHAVIORURUNTIME_API UBehaviorUParallel : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	UBehaviorUParallel();

	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Parallel")
	EBehaviorUParallelPolicy FailurePolicy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Parallel")
	EBehaviorUParallelPolicy SuccessPolicy;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Parallel")
	EBehaviorUChildFinishPolicy ChildFinishPolicy;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUParallelTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviorUParallelTask();

	virtual void Init(UBehaviorUBehaviorNode* InNode) override;
	virtual void Reset(UBehaviorUAgentComponent* Agent) override;

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	/** Status of each child in this parallel execution */
	TArray<EBehaviorUStatus> ChildStatuses;
};

// ===================================================================
// IF-ELSE
// ===================================================================

/**
 * IfElse: expects 3 children: condition, if-true branch, if-false branch.
 */
UCLASS(DisplayName = "IfElse")
class BEHAVIORURUNTIME_API UBehaviorUIfElse : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUIfElseTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// SELECTOR LOOP
// ===================================================================

/**
 * SelectorLoop: a selector that re-evaluates its conditions on each tick.
 * If a higher-priority child becomes valid, it interrupts the current child.
 */
UCLASS(DisplayName = "SelectorLoop")
class BEHAVIORURUNTIME_API UBehaviorUSelectorLoop : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUSelectorLoopTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// SELECTOR PROBABILITY
// ===================================================================

/**
 * SelectorProbability: selects a child randomly based on weight values.
 */
UCLASS(DisplayName = "SelectorProbability")
class BEHAVIORURUNTIME_API UBehaviorUSelectorProbability : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUSelectorProbabilityTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// SELECTOR STOCHASTIC
// ===================================================================

/**
 * SelectorStochastic: a selector that shuffles child order before evaluation.
 */
UCLASS(DisplayName = "SelectorStochastic")
class BEHAVIORURUNTIME_API UBehaviorUSelectorStochastic : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUSelectorStochasticTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviorUSelectorStochasticTask();

	virtual void Reset(UBehaviorUAgentComponent* Agent) override;

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

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
class BEHAVIORURUNTIME_API UBehaviorUSequenceStochastic : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUSequenceStochasticTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviorUSequenceStochasticTask();

	virtual void Reset(UBehaviorUAgentComponent* Agent) override;

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

	TArray<int32> ShuffledOrder;
};

// ===================================================================
// REFERENCE BEHAVIOR (Sub-Tree)
// ===================================================================

/**
 * ReferenceBehavior: references another behavior tree for execution.
 */
UCLASS(DisplayName = "ReferenceBehavior")
class BEHAVIORURUNTIME_API UBehaviorUReferenceBehavior : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** Path to the referenced behavior tree */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Reference")
	FString ReferencedTreePath;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUReferenceBehaviorTask : public UBehaviorUSingleChildTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

	/** The loaded sub-tree task */
	UPROPERTY()
	UBehaviorUBehaviorTreeTask* SubTreeTask;
};

// ===================================================================
// WITH PRECONDITION
// ===================================================================

/**
 * WithPrecondition: expects 2 children. The first is the precondition,
 * the second is the action. Only runs the action if precondition succeeds.
 */
UCLASS(DisplayName = "WithPrecondition")
class BEHAVIORURUNTIME_API UBehaviorUWithPrecondition : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUWithPreconditionTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};
