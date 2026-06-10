// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/BehaviacBehaviorTask.h"
#include "BehaviacTypes.h"
#include "BehaviacFSM.generated.h"

class UBehaviacAgentComponent;

// ===================================================================
// FSM TRANSITION (base)
// ===================================================================

/**
 * Base class for FSM transitions.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class BEHAVIACRUNTIME_API UBehaviacFSMTransition : public UObject
{
	GENERATED_BODY()
public:
	UBehaviacFSMTransition();

	/** Evaluate whether this transition should fire */
	virtual bool Evaluate(UBehaviacAgentComponent* Agent) const;

	/** Target state ID to transition to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	int32 TargetStateId;

	/** Condition expression for this transition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	FString ConditionExpression;

	/** Load from properties */
	virtual void LoadFromProperties(const TArray<FBehaviacProperty>& Properties);
};

/**
 * Standard condition-based transition.
 */
UCLASS(DisplayName = "TransitionCondition")
class BEHAVIACRUNTIME_API UBehaviacTransitionCondition : public UBehaviacFSMTransition
{
	GENERATED_BODY()
public:
	virtual bool Evaluate(UBehaviacAgentComponent* Agent) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	FString LeftOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	FString RightOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	EBehaviacOperatorType Operator;
};

/**
 * Always-true transition (unconditional).
 */
UCLASS(DisplayName = "AlwaysTransition")
class BEHAVIACRUNTIME_API UBehaviacAlwaysTransition : public UBehaviacFSMTransition
{
	GENERATED_BODY()
public:
	virtual bool Evaluate(UBehaviacAgentComponent* Agent) const override { return true; }
};

/**
 * Wait-based transition (transitions after a duration).
 */
UCLASS(DisplayName = "WaitTransition")
class BEHAVIACRUNTIME_API UBehaviacWaitTransition : public UBehaviacFSMTransition
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	float WaitDuration;

	virtual bool Evaluate(UBehaviacAgentComponent* Agent) const override;
};

// ===================================================================
// FSM STATE
// ===================================================================

/**
 * A state within a Finite State Machine.
 * Each state can contain a behavior tree or actions, plus transitions.
 */
UCLASS(DisplayName = "FSM State")
class BEHAVIACRUNTIME_API UBehaviacFSMState : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	UBehaviacFSMState();

	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** Unique state ID within the FSM */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	int32 StateId;

	/** Whether this is the initial state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	bool bIsInitialState;

	/** Whether this is a final/end state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	bool bIsFinalState;

	/** Method to call when entering this state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	FString EnterAction;

	/** Method to call when exiting this state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	FString ExitAction;

	/** Transitions out of this state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "Behaviac|FSM")
	TArray<UBehaviacFSMTransition*> Transitions;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacFSMStateTask : public UBehaviacSingleChildTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual void OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
	/** Routes through OnUpdate even when there is no child node. */
	virtual EBehaviacStatus UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

/**
 * Task for WaitFramesState: returns Running for N frames, then succeeds.
 */
UCLASS()
class BEHAVIACRUNTIME_API UBehaviacWaitFramesStateTask : public UBehaviacFSMStateTask
{
	GENERATED_BODY()
public:
	UBehaviacWaitFramesStateTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	int32 StartFrame;
	int32 TargetFrames;
};

/**
 * Task for WaitState: returns Running for a duration, then succeeds.
 */
UCLASS()
class BEHAVIACRUNTIME_API UBehaviacWaitStateTask : public UBehaviacFSMStateTask
{
	GENERATED_BODY()
public:
	UBehaviacWaitStateTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	double StartTime;
	float WaitDuration;
};

/**
 * WaitFramesState: a state that waits for a number of frames.
 */
UCLASS(DisplayName = "WaitFrames State")
class BEHAVIACRUNTIME_API UBehaviacWaitFramesState : public UBehaviacFSMState
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	int32 WaitFrameCount;
};

/**
 * WaitState: a state that waits for a duration.
 */
UCLASS(DisplayName = "Wait State")
class BEHAVIACRUNTIME_API UBehaviacWaitState : public UBehaviacFSMState
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	float WaitDuration;
};

// ===================================================================
// FSM NODE (container)
// ===================================================================

/**
 * FSM: Finite State Machine container node.
 * Contains multiple states and manages transitions between them.
 */
UCLASS(DisplayName = "FSM")
class BEHAVIACRUNTIME_API UBehaviacFSMNode : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	UBehaviacFSMNode();

	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** ID of the initial state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|FSM")
	int32 InitialStateId;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacFSMTask : public UBehaviacCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviacFSMTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual void OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

	/** Update the FSM: check transitions, switch states */
	EBehaviacStatus UpdateFSM(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus);

	/** Find the task for a given state ID */
	UBehaviacBehaviorTask* FindStateTaskById(int32 StateId) const;

private:
	/** Currently active state index in ChildTasks */
	int32 CurrentStateIndex;
};
