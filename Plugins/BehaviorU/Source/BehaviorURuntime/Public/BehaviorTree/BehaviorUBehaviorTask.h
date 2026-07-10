// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "UObject/NoExportTypes.h"
#include "BehaviorUTypes.h"
#include "BehaviorUBehaviorTask.generated.h"

class UBehaviorUBehaviorNode;
class UBehaviorUAgentComponent;

/**
 * Base class for behavior task instances (runtime state of a behavior node).
 *
 * Mirrors the original behaviac BehaviorTask. Each task holds the runtime
 * execution state for one node instance. The node definition is in
 * UBehaviorUBehaviorNode; the task is the live execution context.
 */
UCLASS(Abstract, BlueprintType)
class BEHAVIORURUNTIME_API UBehaviorUBehaviorTask : public UObject
{
	GENERATED_BODY()

public:
	UBehaviorUBehaviorTask();

	/** Initialize this task with its corresponding node definition */
	virtual void Init(UBehaviorUBehaviorNode* InNode);

	// --- Execution ---

	/** Main execution entry point. Called every tick. */
	EBehaviorUStatus Execute(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus = EBehaviorUStatus::Running);

	/** Get the current status */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|Task")
	EBehaviorUStatus GetStatus() const { return Status; }

	/** Get the node definition this task is running */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|Task")
	UBehaviorUBehaviorNode* GetNode() const { return Node; }

	/** Get the parent task */
	UBehaviorUBehaviorTask* GetParentTask() const { return ParentTask; }

	/** Set the parent task */
	void SetParentTask(UBehaviorUBehaviorTask* InParent) { ParentTask = InParent; }

	/** Reset this task for reuse */
	virtual void Reset(UBehaviorUAgentComponent* Agent);

	/** Traverse the tree to reset all running/completed tasks */
	virtual void Traverse(bool bChildFirst, TFunction<bool(UBehaviorUBehaviorTask*)> Handler);

protected:
	/** Called when entering this node */
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent);

	/** Called when exiting this node */
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus);

	/** Main update logic. Override in subclasses. */
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus);

	/** Wrapper that calls OnUpdate and handles preconditions/effectors */
	virtual EBehaviorUStatus UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus);

	/** Check preconditions for the enter phase */
	bool CheckPreconditions(UBehaviorUAgentComponent* Agent, bool bIsUpdate) const;

	/** Apply effectors based on result */
	void ApplyEffectors(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) const;

	/** The node definition */
	UPROPERTY()
	UBehaviorUBehaviorNode* Node;

	/** Current status */
	UPROPERTY()
	EBehaviorUStatus Status;

	/** Parent task */
	UPROPERTY()
	UBehaviorUBehaviorTask* ParentTask;

	/** Has this task been entered? */
	bool bHasEntered;
};

// -------------------------------------------------------------------
// Composite Task: Base for nodes that manage multiple children
// -------------------------------------------------------------------
UCLASS(Abstract)
class BEHAVIORURUNTIME_API UBehaviorUCompositeTask : public UBehaviorUBehaviorTask
{
	GENERATED_BODY()

public:
	UBehaviorUCompositeTask();

	virtual void Init(UBehaviorUBehaviorNode* InNode) override;
	virtual void Reset(UBehaviorUAgentComponent* Agent) override;
	virtual void Traverse(bool bChildFirst, TFunction<bool(UBehaviorUBehaviorTask*)> Handler) override;

	/** Get all child tasks */
	const TArray<UBehaviorUBehaviorTask*>& GetChildTasks() const { return ChildTasks; }

protected:
	/** Index of the currently active child */
	UPROPERTY()
	int32 ActiveChildIndex;

	/** Child task instances */
	UPROPERTY()
	TArray<UBehaviorUBehaviorTask*> ChildTasks;
};

// -------------------------------------------------------------------
// Single Child Task: Base for nodes with exactly one child (decorators)
// -------------------------------------------------------------------
UCLASS(Abstract)
class BEHAVIORURUNTIME_API UBehaviorUSingleChildTask : public UBehaviorUBehaviorTask
{
	GENERATED_BODY()

public:
	UBehaviorUSingleChildTask();

	virtual void Init(UBehaviorUBehaviorNode* InNode) override;
	virtual void Reset(UBehaviorUAgentComponent* Agent) override;
	virtual void Traverse(bool bChildFirst, TFunction<bool(UBehaviorUBehaviorTask*)> Handler) override;

protected:
	virtual EBehaviorUStatus UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

	/** The single child task */
	UPROPERTY()
	UBehaviorUBehaviorTask* ChildTask;
};

// -------------------------------------------------------------------
// Leaf Task: Base for nodes with no children (actions, conditions)
// -------------------------------------------------------------------
UCLASS(Abstract)
class BEHAVIORURUNTIME_API UBehaviorULeafTask : public UBehaviorUBehaviorTask
{
	GENERATED_BODY()

public:
	// Leaf tasks have no children, so Init/Traverse are simple
};

// -------------------------------------------------------------------
// BehaviorTree Task: Root-level task wrapping the entire tree
// -------------------------------------------------------------------
UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUBehaviorTreeTask : public UBehaviorUSingleChildTask
{
	GENERATED_BODY()

public:
	/** Override Init to create task from root node itself */
	virtual void Init(UBehaviorUBehaviorNode* InNode) override;
	
	/** Tick the entire behavior tree */
	EBehaviorUStatus Tick(UBehaviorUAgentComponent* Agent);

	/** Get the tree-level status */
	UFUNCTION(BlueprintCallable, Category = "BehaviorU|BehaviorTree")
	EBehaviorUStatus GetTreeStatus() const { return Status; }
	
	/** Check if child task was created */
	bool HasChildTask() const { return ChildTask != nullptr; }

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};
