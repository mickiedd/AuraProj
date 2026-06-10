// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/BehaviacBehaviorTask.h"
#include "BehaviacActions.generated.h"

class UBehaviacAgentComponent;

// ===================================================================
// ACTION
// ===================================================================

/**
 * Action: calls a method on the agent. The most common leaf node.
 */
UCLASS(DisplayName = "Action")
class BEHAVIACRUNTIME_API UBehaviacAction : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** The method name to call on the agent */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Action")
	FString MethodName;

	/** Result status when method is called */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Action")
	EBehaviacStatus ResultOption;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacActionTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
public:
	/** 重置时同步清理挂起命令状态，防止 ResetBehaviorTree() 后状态残留 */
	virtual void Reset(UBehaviacAgentComponent* Agent) override;

protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
	virtual void OnExit(UBehaviacAgentComponent* Agent, EBehaviacStatus InStatus) override;

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
class BEHAVIACRUNTIME_API UBehaviacAssignment : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Assignment")
	FString PropertyName;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Assignment")
	FString PropertyValue;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Assignment")
	bool bCastFromRight;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacAssignmentTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// COMPUTE
// ===================================================================

/**
 * Compute: performs an arithmetic operation and stores the result.
 */
UCLASS(DisplayName = "Compute")
class BEHAVIACRUNTIME_API UBehaviacCompute : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Compute")
	FString ResultProperty;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Compute")
	FString LeftOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Compute")
	FString RightOperand;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Compute")
	EBehaviacOperatorType Operator;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacComputeTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// NOOP
// ===================================================================

/**
 * Noop: does nothing, always succeeds. Useful as a placeholder.
 */
UCLASS(DisplayName = "Noop")
class BEHAVIACRUNTIME_API UBehaviacNoop : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacNoopTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// END
// ===================================================================

/**
 * End: terminates the behavior tree with a specific status.
 */
UCLASS(DisplayName = "End")
class BEHAVIACRUNTIME_API UBehaviacEnd : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|End")
	EBehaviacStatus EndStatus;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|End")
	bool bEndOutermost;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacEndTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// WAIT
// ===================================================================

/**
 * Wait: returns Running for a specified duration (seconds), then succeeds.
 */
UCLASS(DisplayName = "Wait")
class BEHAVIACRUNTIME_API UBehaviacWait : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Wait")
	float Duration;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacWaitTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
public:
	UBehaviacWaitTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

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
class BEHAVIACRUNTIME_API UBehaviacWaitFrames : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|WaitFrames")
	int32 FrameCount;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacWaitFramesTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
public:
	UBehaviacWaitFramesTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

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
class BEHAVIACRUNTIME_API UBehaviacWaitForSignal : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|WaitForSignal")
	FString SignalName;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacWaitForSignalTask : public UBehaviacLeafTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};
