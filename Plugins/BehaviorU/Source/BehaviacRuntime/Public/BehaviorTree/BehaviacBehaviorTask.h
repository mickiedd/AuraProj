// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BehaviacTypes.h"
#include "BehaviacBehaviorTask.generated.h"

class UBehaviacBehaviorNode;
class UBehaviacAgentComponent;

/**
 * Base class for behavior task instances (runtime state of a behavior node).
 *
 * Mirrors the original behaviac BehaviorTask. Each task holds the runtime
 * execution state for one node instance. The node definition is in
 * UBehaviacBehaviorNode; the task is the live execution context.
 */
UCLASS(Abstract, BlueprintType)
class BEHAVIACRUNTIME_API UBehaviacBehaviorTask : public UObject
{
	GENERATED_BODY()

public:
	UBehaviacBehaviorTask();

	/** Initialize this task with its corresponding node definition */
	virtual void Init(UBehaviacBehaviorNode* InNode);

	// --- Execution ---

	/** Main execution entry point. Called every tick. */
	EBehaviacStatus Execute(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus = EBehaviacStatus::Running);

	/** Get the current status */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Task")
	EBehaviacStatus GetStatus() const { return Status; }

	/** Get the node definition this task is running */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|Task")
	UBehaviacBehaviorNode* GetNode() const { return Node; }

	/** Get the parent task */
	UBehaviacBehaviorTask* GetParentTask() const { return ParentTask; }

	/** Set the parent task */
	void SetParentTask(UBehaviacBehaviorTask* InParent) { ParentTask = InParent; }

	/** Reset this task for reuse */
	virtual void Reset(UBehaviacAgentComponent* Agent);

	/** Traverse the tree to reset all running/completed tasks */
	virtual void Traverse(bool bChildFirst, TFunction<bool(UBehaviacBehaviorTask*)> Handler);

protected:
	/** Called when entering this node */
	virtual bool OnEnter(UBehaviacAgentComponent* Agent);

	/** Called when exiting this node */
	virtual void OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus);

	/** Main update logic. Override in subclasses. */
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus);

	/** Wrapper that calls OnUpdate and handles preconditions/effectors */
	virtual EBehaviacStatus UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus);

	/** Check preconditions for the enter phase */
	bool CheckPreconditions(UBehaviacAgentComponent* Agent, bool bIsUpdate) const;

	/** Apply effectors based on result */
	void ApplyEffectors(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) const;

	/** The node definition */
	UPROPERTY()
	UBehaviacBehaviorNode* Node;

	/** Current status */
	UPROPERTY()
	EBehaviacStatus Status;

	/** Parent task */
	UPROPERTY()
	UBehaviacBehaviorTask* ParentTask;

	/** Has this task been entered? */
	bool bHasEntered;
};

// -------------------------------------------------------------------
// Composite Task: Base for nodes that manage multiple children
// -------------------------------------------------------------------
UCLASS(Abstract)
class BEHAVIACRUNTIME_API UBehaviacCompositeTask : public UBehaviacBehaviorTask
{
	GENERATED_BODY()

public:
	UBehaviacCompositeTask();

	virtual void Init(UBehaviacBehaviorNode* InNode) override;
	virtual void Reset(UBehaviacAgentComponent* Agent) override;
	virtual void Traverse(bool bChildFirst, TFunction<bool(UBehaviacBehaviorTask*)> Handler) override;

	/** Get all child tasks */
	const TArray<UBehaviacBehaviorTask*>& GetChildTasks() const { return ChildTasks; }

protected:
	/** Index of the currently active child */
	UPROPERTY()
	int32 ActiveChildIndex;

	/** Child task instances */
	UPROPERTY()
	TArray<UBehaviacBehaviorTask*> ChildTasks;
};

// -------------------------------------------------------------------
// Single Child Task: Base for nodes with exactly one child (decorators)
// -------------------------------------------------------------------
UCLASS(Abstract)
class BEHAVIACRUNTIME_API UBehaviacSingleChildTask : public UBehaviacBehaviorTask
{
	GENERATED_BODY()

public:
	UBehaviacSingleChildTask();

	virtual void Init(UBehaviacBehaviorNode* InNode) override;
	virtual void Reset(UBehaviacAgentComponent* Agent) override;
	virtual void Traverse(bool bChildFirst, TFunction<bool(UBehaviacBehaviorTask*)> Handler) override;

protected:
	virtual EBehaviacStatus UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

	/** The single child task */
	UPROPERTY()
	UBehaviacBehaviorTask* ChildTask;
};

// -------------------------------------------------------------------
// Leaf Task: Base for nodes with no children (actions, conditions)
// -------------------------------------------------------------------
UCLASS(Abstract)
class BEHAVIACRUNTIME_API UBehaviacLeafTask : public UBehaviacBehaviorTask
{
	GENERATED_BODY()

public:
	// Leaf tasks have no children, so Init/Traverse are simple
};

// -------------------------------------------------------------------
// BehaviorTree Task: Root-level task wrapping the entire tree
// -------------------------------------------------------------------
UCLASS()
class BEHAVIACRUNTIME_API UBehaviacBehaviorTreeTask : public UBehaviacSingleChildTask
{
	GENERATED_BODY()

public:
	/** Override Init to create task from root node itself */
	virtual void Init(UBehaviacBehaviorNode* InNode) override;
	
	/** Tick the entire behavior tree */
	EBehaviacStatus Tick(UBehaviacAgentComponent* Agent);

	/** Get the tree-level status */
	UFUNCTION(BlueprintCallable, Category = "Behaviac|BehaviorTree")
	EBehaviacStatus GetTreeStatus() const { return Status; }
	
	/** Check if child task was created */
	bool HasChildTask() const { return ChildTask != nullptr; }

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual void OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};
