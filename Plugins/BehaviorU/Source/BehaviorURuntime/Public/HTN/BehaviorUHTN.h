// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorUTypes.h"
#include "BehaviorUHTN.generated.h"

class UBehaviorUAgentComponent;

// ===================================================================
// HTN TASK (primitive task)
// ===================================================================

/**
 * HTN Task: A primitive task that can be decomposed or executed directly.
 */
UCLASS(DisplayName = "HTN Task")
class BEHAVIORURUNTIME_API UBehaviorUHTNTask : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** Whether this is a primitive (directly executable) task */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|HTN")
	bool bIsPrimitive;

	/** Referenced behavior tree path (for compound tasks) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|HTN")
	FString ReferencedTreePath;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUHTNTaskExecution : public UBehaviorUSingleChildTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// HTN METHOD
// ===================================================================

/**
 * HTN Method: defines one way to decompose a compound task.
 * A compound task may have multiple methods (alternative decompositions).
 */
UCLASS(DisplayName = "HTN Method")
class BEHAVIORURUNTIME_API UBehaviorUHTNMethod : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** Precondition for this method to be applicable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|HTN")
	FString MethodPrecondition;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUHTNMethodTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// HTN PLANNER
// ===================================================================

/**
 * HTN Planner: generates and executes plans using Hierarchical Task Network decomposition.
 *
 * The planner takes a root task and decomposes compound tasks into primitive tasks
 * until a complete plan is found. It then executes the plan and can automatically
 * replan when the plan fails or the world state changes.
 */
UCLASS(BlueprintType)
class BEHAVIORURUNTIME_API UBehaviorUHTNPlanner : public UObject
{
	GENERATED_BODY()
public:
	UBehaviorUHTNPlanner();

	/** Initialize the planner with an agent and root task */
	void Init(UBehaviorUAgentComponent* InAgent, UBehaviorUHTNTask* InRootTask);

	/** Uninitialize the planner */
	void Uninit();

	/** Update the planner (tick) */
	EBehaviorUStatus Update();

	/** Whether auto-replanning is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|HTN")
	bool bAutoReplan;

private:
	/** Generate a plan from the current state */
	bool GeneratePlan();

	/** Check if the current plan can be interrupted */
	bool CanInterruptCurrentPlan() const;

	/** Execute the current plan */
	EBehaviorUStatus ExecutePlan();

	/** Decompose a task into a sequence of primitive tasks */
	bool DecomposeTask(UBehaviorUHTNTask* Task, TArray<UBehaviorUHTNTask*>& OutPlan, int32 Depth);

	/** The agent being planned for */
	UPROPERTY()
	UBehaviorUAgentComponent* Agent;

	/** Root task node */
	UPROPERTY()
	UBehaviorUHTNTask* RootTaskNode;

	/** Current plan (sequence of primitive tasks) */
	UPROPERTY()
	TArray<UBehaviorUHTNTask*> CurrentPlan;

	/** Current step in the plan */
	int32 CurrentPlanStep;

	/** Current task execution */
	UPROPERTY()
	UBehaviorUBehaviorTask* CurrentTaskExecution;

	/** Maximum decomposition depth to prevent infinite recursion */
	static constexpr int32 MAX_DECOMPOSITION_DEPTH = 256;
};
