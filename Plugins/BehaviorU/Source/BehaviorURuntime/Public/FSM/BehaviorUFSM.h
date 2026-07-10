// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorUTypes.h"
#include "BehaviorUFSM.generated.h"

class UBehaviorUAgentComponent;

// ===================================================================
// FSM TRANSITION (base)
// ===================================================================

/**
 * Base class for FSM transitions.
 */
UCLASS(Abstract, Blueprintable, EditInlineNew, DefaultToInstanced)
class BEHAVIORURUNTIME_API UBehaviorUFSMTransition : public UObject
{
	GENERATED_BODY()
public:
	UBehaviorUFSMTransition();

	/** Evaluate whether this transition should fire */
	virtual bool Evaluate(UBehaviorUAgentComponent* Agent) const;

	/** Target state ID to transition to */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	int32 TargetStateId;

	/** Condition expression for this transition */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	FString ConditionExpression;

	/** Load from properties */
	virtual void LoadFromProperties(const TArray<FBehaviorUProperty>& Properties);

	/**
	 * Called by the FSM when the owning state is (re-)entered.
	 * Override in subclasses that need to reset internal timers/counters.
	 * Default implementation does nothing.
	 */
	virtual void OnStateEntered(UBehaviorUAgentComponent* Agent) {}
};

/**
 * Standard condition-based transition.
 */
UCLASS(DisplayName = "TransitionCondition")
class BEHAVIORURUNTIME_API UBehaviorUTransitionCondition : public UBehaviorUFSMTransition
{
	GENERATED_BODY()
public:
	virtual bool Evaluate(UBehaviorUAgentComponent* Agent) const override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	FString LeftOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	FString RightOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	EBehaviorUOperatorType Operator;
};

/**
 * Always-true transition (unconditional).
 */
UCLASS(DisplayName = "AlwaysTransition")
class BEHAVIORURUNTIME_API UBehaviorUAlwaysTransition : public UBehaviorUFSMTransition
{
	GENERATED_BODY()
public:
	virtual bool Evaluate(UBehaviorUAgentComponent* Agent) const override { return true; }
};

/**
 * Wait-based transition (transitions after a duration).
 */
UCLASS(DisplayName = "WaitTransition")
class BEHAVIORURUNTIME_API UBehaviorUWaitTransition : public UBehaviorUFSMTransition
{
	GENERATED_BODY()
public:
	UBehaviorUWaitTransition();

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	float WaitDuration;

	virtual bool Evaluate(UBehaviorUAgentComponent* Agent) const override;

	/** Load WaitDuration from XML properties. */
	virtual void LoadFromProperties(const TArray<FBehaviorUProperty>& Properties) override;

	/** Reset the internal timer using the agent's world clock.
	 *  Must be called when the owning state is (re-)entered.
	 *  Uses GetTimeSeconds() when Agent has a World, matching Evaluate(). */
	void ResetTimer(UBehaviorUAgentComponent* Agent);

	/** Called by the FSM when the owning state is entered — resets the timer. */
	virtual void OnStateEntered(UBehaviorUAgentComponent* Agent) override { ResetTimer(Agent); }

private:
	/** When the timer started (world time seconds). mutable: Evaluate is const. */
	mutable double StartTime = 0.0;
	/** Whether the timer has been initialised for the current state. mutable: same reason. */
	mutable bool bTimerStarted = false;
};

// ===================================================================
// FSM STATE
// ===================================================================

/**
 * A state within a Finite State Machine.
 * Each state can contain a behavior tree or actions, plus transitions.
 */
UCLASS(DisplayName = "FSM State")
class BEHAVIORURUNTIME_API UBehaviorUFSMState : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	UBehaviorUFSMState();

	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** Unique state ID within the FSM */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	int32 StateId;

	/** Whether this is the initial state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	bool bIsInitialState;

	/** Whether this is a final/end state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	bool bIsFinalState;

	/** Method to call when entering this state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	FString EnterAction;

	/** Method to call when exiting this state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	FString ExitAction;

	/** Transitions out of this state */
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "BehaviorU|FSM")
	TArray<UBehaviorUFSMTransition*> Transitions;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUFSMStateTask : public UBehaviorUSingleChildTask
{
	GENERATED_BODY()
public:
	/** Fire ExitAction via OnExit. Public so UBehaviorUFSMTask can trigger it
	 *  on state transitions (OnExit is protected on the base class). */
	void ExitState(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus)
	{
		OnExit(Agent, InStatus);
	}

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
	/** Routes through OnUpdate even when there is no child node. */
	virtual EBehaviorUStatus UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

/**
 * Task for WaitFramesState: returns Running for N frames, then succeeds.
 */
UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUWaitFramesStateTask : public UBehaviorUFSMStateTask
{
	GENERATED_BODY()
public:
	UBehaviorUWaitFramesStateTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	int32 StartFrame;
	int32 TargetFrames;
};

/**
 * Task for WaitState: returns Running for a duration, then succeeds.
 */
UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUWaitStateTask : public UBehaviorUFSMStateTask
{
	GENERATED_BODY()
public:
	UBehaviorUWaitStateTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	double StartTime;
	float WaitDuration;
};

/**
 * WaitFramesState: a state that waits for a number of frames.
 */
UCLASS(DisplayName = "WaitFrames State")
class BEHAVIORURUNTIME_API UBehaviorUWaitFramesState : public UBehaviorUFSMState
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	int32 WaitFrameCount;
};

/**
 * WaitState: a state that waits for a duration.
 */
UCLASS(DisplayName = "Wait State")
class BEHAVIORURUNTIME_API UBehaviorUWaitState : public UBehaviorUFSMState
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
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
class BEHAVIORURUNTIME_API UBehaviorUFSMNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	UBehaviorUFSMNode();

	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** ID of the initial state */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|FSM")
	int32 InitialStateId;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUFSMTask : public UBehaviorUCompositeTask
{
	GENERATED_BODY()
public:
	UBehaviorUFSMTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

	/** Update the FSM: check transitions, switch states */
	EBehaviorUStatus UpdateFSM(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus);

	/** Find the task for a given state ID */
	UBehaviorUBehaviorTask* FindStateTaskById(int32 StateId) const;

private:
	/** Currently active state index in ChildTasks */
	int32 CurrentStateIndex;
};
