// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/BehaviacBehaviorTask.h"
#include "BehaviacTypes.h"
#include "BehaviacHTN.generated.h"

class UBehaviacAgentComponent;

// ===================================================================
// HTN TASK (primitive task)
// ===================================================================

/**
 * HTN Task: A primitive task that can be decomposed or executed directly.
 */
UCLASS(DisplayName = "HTN Task")
class BEHAVIACRUNTIME_API UBehaviacHTNTask : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** Whether this is a primitive (directly executable) task */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|HTN")
	bool bIsPrimitive;

	/** Referenced behavior tree path (for compound tasks) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|HTN")
	FString ReferencedTreePath;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacHTNTaskExecution : public UBehaviacSingleChildTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// HTN METHOD
// ===================================================================

/**
 * HTN Method: defines one way to decompose a compound task.
 * A compound task may have multiple methods (alternative decompositions).
 */
UCLASS(DisplayName = "HTN Method")
class BEHAVIACRUNTIME_API UBehaviacHTNMethod : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** Precondition for this method to be applicable */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|HTN")
	FString MethodPrecondition;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacHTNMethodTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
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
class BEHAVIACRUNTIME_API UBehaviacHTNPlanner : public UObject
{
	GENERATED_BODY()
public:
	UBehaviacHTNPlanner();

	/** Initialize the planner with an agent and root task */
	void Init(UBehaviacAgentComponent* InAgent, UBehaviacHTNTask* InRootTask);

	/** Uninitialize the planner */
	void Uninit();

	/** Update the planner (tick) */
	EBehaviacStatus Update();

	/** Whether auto-replanning is enabled */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|HTN")
	bool bAutoReplan;

private:
	/** Generate a plan from the current state */
	bool GeneratePlan();

	/** Check if the current plan can be interrupted */
	bool CanInterruptCurrentPlan() const;

	/** Execute the current plan */
	EBehaviacStatus ExecutePlan();

	/** Decompose a task into a sequence of primitive tasks */
	bool DecomposeTask(UBehaviacHTNTask* Task, TArray<UBehaviacHTNTask*>& OutPlan, int32 Depth);

	/** The agent being planned for */
	UPROPERTY()
	UBehaviacAgentComponent* Agent;

	/** Root task node */
	UPROPERTY()
	UBehaviacHTNTask* RootTaskNode;

	/** Current plan (sequence of primitive tasks) */
	UPROPERTY()
	TArray<UBehaviacHTNTask*> CurrentPlan;

	/** Current step in the plan */
	int32 CurrentPlanStep;

	/** Current task execution */
	UPROPERTY()
	UBehaviacBehaviorTask* CurrentTaskExecution;

	/** Maximum decomposition depth to prevent infinite recursion */
	static constexpr int32 MAX_DECOMPOSITION_DEPTH = 256;
};
