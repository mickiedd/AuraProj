// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorUActions.generated.h"

class UBehaviorUAgentComponent;

// ===================================================================
// ACTION
// ===================================================================

/**
 * Action: calls a method on the agent. The most common leaf node.
 */
UCLASS(DisplayName = "Action")
class BEHAVIORURUNTIME_API UBehaviorUAction : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** The method name to call on the agent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Action")
	FString MethodName;

	/** Result status when method is called */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Action")
	EBehaviorUStatus ResultOption;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUActionTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
public:
	/** 重置时同步清理挂起命令状态，防止 ResetBehaviorTree() 后状态残留 */
	virtual void Reset(UBehaviorUAgentComponent* Agent) override;

protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;

private:
	/** 当前挂起命令的唯一 ID（0 表示无挂起命令） */
	uint64 PendingCommandId = 0;

	/** 是否已向队列提交了本次方法命令，等待结果返回 */
	bool bHasPendingCommand = false;
};

// ===================================================================
// ASSIGNMENT
// ===================================================================

/**
 * Assignment: sets a property value on the agent.
 */
UCLASS(DisplayName = "Assignment")
class BEHAVIORURUNTIME_API UBehaviorUAssignment : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Assignment")
	FString PropertyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Assignment")
	FString PropertyValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Assignment")
	bool bCastFromRight;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUAssignmentTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// COMPUTE
// ===================================================================

/**
 * Compute: performs an arithmetic operation and stores the result.
 */
UCLASS(DisplayName = "Compute")
class BEHAVIORURUNTIME_API UBehaviorUCompute : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Compute")
	FString ResultProperty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Compute")
	FString LeftOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Compute")
	FString RightOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Compute")
	EBehaviorUOperatorType Operator;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUComputeTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// NOOP
// ===================================================================

/**
 * Noop: does nothing, always succeeds. Useful as a placeholder.
 */
UCLASS(DisplayName = "Noop")
class BEHAVIORURUNTIME_API UBehaviorUNoop : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUNoopTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// END
// ===================================================================

/**
 * End: terminates the behavior tree with a specific status.
 */
UCLASS(DisplayName = "End")
class BEHAVIORURUNTIME_API UBehaviorUEnd : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|End")
	EBehaviorUStatus EndStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|End")
	bool bEndOutermost;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUEndTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// WAIT
// ===================================================================

/**
 * Wait: returns Running for a specified duration (seconds), then succeeds.
 */
UCLASS(DisplayName = "Wait")
class BEHAVIORURUNTIME_API UBehaviorUWait : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Wait")
	float Duration;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUWaitTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
public:
	UBehaviorUWaitTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	double StartTime;
	float WaitDuration;
};

// ===================================================================
// WAIT FRAMES
// ===================================================================

/**
 * WaitFrames: returns Running for a specified number of frames, then succeeds.
 */
UCLASS(DisplayName = "WaitFrames")
class BEHAVIORURUNTIME_API UBehaviorUWaitFrames : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|WaitFrames")
	int32 FrameCount;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUWaitFramesTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
public:
	UBehaviorUWaitFramesTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	int32 StartFrame;
	int32 TargetFrames;
};

// ===================================================================
// WAIT FOR SIGNAL
// ===================================================================

/**
 * WaitForSignal: returns Running until a named signal is received.
 */
UCLASS(DisplayName = "WaitForSignal")
class BEHAVIORURUNTIME_API UBehaviorUWaitForSignal : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|WaitForSignal")
	FString SignalName;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUWaitForSignalTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};
