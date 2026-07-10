// BehaviorU UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "BehaviorUDecorators.generated.h"

class UBehaviorUAgentComponent;

// ===================================================================
// BASE DECORATOR
// ===================================================================

/** Base class for all decorator nodes. Decorators wrap a single child. */
UCLASS(Abstract, DisplayName = "Decorator")
class BEHAVIORURUNTIME_API UBehaviorUDecorator : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	/** Whether to negate the child's result */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	bool bDecorateWhenChildEnds;
};

/** Base task for decorators */
UCLASS(Abstract)
class BEHAVIORURUNTIME_API UBehaviorUDecoratorTask : public UBehaviorUSingleChildTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
	virtual EBehaviorUStatus UpdateCurrent(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
	virtual EBehaviorUStatus DecorateResult(EBehaviorUStatus ChildResult);
};

// ===================================================================
// DECORATOR: AlwaysFailure
// ===================================================================

UCLASS(DisplayName = "AlwaysFailure")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorAlwaysFailure : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorAlwaysFailureTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus DecorateResult(EBehaviorUStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: AlwaysRunning
// ===================================================================

UCLASS(DisplayName = "AlwaysRunning")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorAlwaysRunning : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorAlwaysRunningTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus DecorateResult(EBehaviorUStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: AlwaysSuccess
// ===================================================================

UCLASS(DisplayName = "AlwaysSuccess")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorAlwaysSuccess : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorAlwaysSuccessTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus DecorateResult(EBehaviorUStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: Not (Inverter)
// ===================================================================

UCLASS(DisplayName = "Not")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorNot : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorNotTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus DecorateResult(EBehaviorUStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: Loop
// ===================================================================

UCLASS(DisplayName = "Loop")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorLoop : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** Number of times to loop. -1 means infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	int32 LoopCount;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorLoopTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviorUDecoratorLoopTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	int32 CurrentCount;
	int32 TargetCount;
};

// ===================================================================
// DECORATOR: LoopUntil
// ===================================================================

UCLASS(DisplayName = "LoopUntil")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorLoopUntil : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	/** Loop until child returns this status */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	bool bUntilSuccess;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorLoopUntilTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// DECORATOR: Repeat
// ===================================================================

UCLASS(DisplayName = "Repeat")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorRepeat : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	int32 RepeatCount;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorRepeatTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviorUDecoratorRepeatTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: Count
// ===================================================================

UCLASS(DisplayName = "Count")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorCount : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	int32 CountLimit;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorCountTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviorUDecoratorCountTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: CountLimit
// ===================================================================

UCLASS(DisplayName = "CountLimit")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorCountLimit : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	int32 CountMax;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorCountLimitTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviorUDecoratorCountLimitTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;

private:
	int32 ExecutionCount;
};

// ===================================================================
// DECORATOR: Time
// ===================================================================

UCLASS(DisplayName = "DecoratorTime")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorTime : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	float TimeDuration;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorTimeTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	double StartTime;
};

// ===================================================================
// DECORATOR: Frames
// ===================================================================

UCLASS(DisplayName = "DecoratorFrames")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorFrames : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	int32 FrameCount;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorFramesTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	int32 StartFrame;
};

// ===================================================================
// DECORATOR: FailureUntil
// ===================================================================

UCLASS(DisplayName = "FailureUntil")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorFailureUntil : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	int32 UntilCount;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorFailureUntilTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviorUDecoratorFailureUntilTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus DecorateResult(EBehaviorUStatus ChildResult) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: SuccessUntil
// ===================================================================

UCLASS(DisplayName = "SuccessUntil")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorSuccessUntil : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	int32 UntilCount;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorSuccessUntilTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviorUDecoratorSuccessUntilTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus DecorateResult(EBehaviorUStatus ChildResult) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: Iterator
// ===================================================================

UCLASS(DisplayName = "Iterator")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorIterator : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	FString ArrayProperty;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorIteratorTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviorUDecoratorIteratorTask();

protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;

private:
	int32 CurrentIndex;
	int32 ArrayCount;
};

// ===================================================================
// DECORATOR: Log
// ===================================================================

UCLASS(DisplayName = "Log")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorLog : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	FString LogMessage;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorLogTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};

// ===================================================================
// DECORATOR: Weight (used with SelectorProbability)
// ===================================================================

UCLASS(DisplayName = "Weight")
class BEHAVIORURUNTIME_API UBehaviorUDecoratorWeight : public UBehaviorUDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "BehaviorU|Decorator")
	float Weight;
};

UCLASS()
class BEHAVIORURUNTIME_API UBehaviorUDecoratorWeightTask : public UBehaviorUDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
};
