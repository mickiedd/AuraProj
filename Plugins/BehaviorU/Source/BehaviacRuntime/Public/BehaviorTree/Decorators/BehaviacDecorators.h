// Behaviac UE5 Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviacBehaviorNode.h"
#include "BehaviorTree/BehaviacBehaviorTask.h"
#include "BehaviacDecorators.generated.h"

class UBehaviacAgentComponent;

// ===================================================================
// BASE DECORATOR
// ===================================================================

/** Base class for all decorator nodes. Decorators wrap a single child. */
UCLASS(Abstract, DisplayName = "Decorator")
class BEHAVIACRUNTIME_API UBehaviacDecorator : public UBehaviacBehaviorNode
{
	GENERATED_BODY()
public:
	/** Whether to negate the child's result */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	bool bDecorateWhenChildEnds;
};

/** Base task for decorators */
UCLASS(Abstract)
class BEHAVIACRUNTIME_API UBehaviacDecoratorTask : public UBehaviacSingleChildTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
	virtual EBehaviacStatus UpdateCurrent(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
	virtual EBehaviacStatus DecorateResult(EBehaviacStatus ChildResult);
};

// ===================================================================
// DECORATOR: AlwaysFailure
// ===================================================================

UCLASS(DisplayName = "AlwaysFailure")
class BEHAVIACRUNTIME_API UBehaviacDecoratorAlwaysFailure : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorAlwaysFailureTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus DecorateResult(EBehaviacStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: AlwaysRunning
// ===================================================================

UCLASS(DisplayName = "AlwaysRunning")
class BEHAVIACRUNTIME_API UBehaviacDecoratorAlwaysRunning : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorAlwaysRunningTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus DecorateResult(EBehaviacStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: AlwaysSuccess
// ===================================================================

UCLASS(DisplayName = "AlwaysSuccess")
class BEHAVIACRUNTIME_API UBehaviacDecoratorAlwaysSuccess : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorAlwaysSuccessTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus DecorateResult(EBehaviacStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: Not (Inverter)
// ===================================================================

UCLASS(DisplayName = "Not")
class BEHAVIACRUNTIME_API UBehaviacDecoratorNot : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorNotTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus DecorateResult(EBehaviacStatus ChildResult) override;
};

// ===================================================================
// DECORATOR: Loop
// ===================================================================

UCLASS(DisplayName = "Loop")
class BEHAVIACRUNTIME_API UBehaviacDecoratorLoop : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** Number of times to loop. -1 means infinite. */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	int32 LoopCount;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorLoopTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviacDecoratorLoopTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	int32 CurrentCount;
	int32 TargetCount;
};

// ===================================================================
// DECORATOR: LoopUntil
// ===================================================================

UCLASS(DisplayName = "LoopUntil")
class BEHAVIACRUNTIME_API UBehaviacDecoratorLoopUntil : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	/** Loop until child returns this status */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	bool bUntilSuccess;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorLoopUntilTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// DECORATOR: Repeat
// ===================================================================

UCLASS(DisplayName = "Repeat")
class BEHAVIACRUNTIME_API UBehaviacDecoratorRepeat : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	int32 RepeatCount;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorRepeatTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviacDecoratorRepeatTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: Count
// ===================================================================

UCLASS(DisplayName = "Count")
class BEHAVIACRUNTIME_API UBehaviacDecoratorCount : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	int32 CountLimit;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorCountTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviacDecoratorCountTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: CountLimit
// ===================================================================

UCLASS(DisplayName = "CountLimit")
class BEHAVIACRUNTIME_API UBehaviacDecoratorCountLimit : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	int32 CountMax;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorCountLimitTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviacDecoratorCountLimitTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;

private:
	int32 ExecutionCount;
};

// ===================================================================
// DECORATOR: Time
// ===================================================================

UCLASS(DisplayName = "DecoratorTime")
class BEHAVIACRUNTIME_API UBehaviacDecoratorTime : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	float TimeDuration;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorTimeTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	double StartTime;
};

// ===================================================================
// DECORATOR: Frames
// ===================================================================

UCLASS(DisplayName = "DecoratorFrames")
class BEHAVIACRUNTIME_API UBehaviacDecoratorFrames : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	int32 FrameCount;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorFramesTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	int32 StartFrame;
};

// ===================================================================
// DECORATOR: FailureUntil
// ===================================================================

UCLASS(DisplayName = "FailureUntil")
class BEHAVIACRUNTIME_API UBehaviacDecoratorFailureUntil : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	int32 UntilCount;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorFailureUntilTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviacDecoratorFailureUntilTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus DecorateResult(EBehaviacStatus ChildResult) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: SuccessUntil
// ===================================================================

UCLASS(DisplayName = "SuccessUntil")
class BEHAVIACRUNTIME_API UBehaviacDecoratorSuccessUntil : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	int32 UntilCount;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorSuccessUntilTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviacDecoratorSuccessUntilTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus DecorateResult(EBehaviacStatus ChildResult) override;

private:
	int32 CurrentCount;
};

// ===================================================================
// DECORATOR: Iterator
// ===================================================================

UCLASS(DisplayName = "Iterator")
class BEHAVIACRUNTIME_API UBehaviacDecoratorIterator : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	FString ArrayProperty;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorIteratorTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
public:
	UBehaviacDecoratorIteratorTask();

protected:
	virtual bool OnEnter(UBehaviacAgentComponent* Agent) override;
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;

private:
	int32 CurrentIndex;
	int32 ArrayCount;
};

// ===================================================================
// DECORATOR: Log
// ===================================================================

UCLASS(DisplayName = "Log")
class BEHAVIACRUNTIME_API UBehaviacDecoratorLog : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	FString LogMessage;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorLogTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};

// ===================================================================
// DECORATOR: Weight (used with SelectorProbability)
// ===================================================================

UCLASS(DisplayName = "Weight")
class BEHAVIACRUNTIME_API UBehaviacDecoratorWeight : public UBehaviacDecorator
{
	GENERATED_BODY()
public:
	virtual UBehaviacBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviacProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Behaviac|Decorator")
	float Weight;
};

UCLASS()
class BEHAVIACRUNTIME_API UBehaviacDecoratorWeightTask : public UBehaviacDecoratorTask
{
	GENERATED_BODY()
protected:
	virtual EBehaviacStatus OnUpdate(UBehaviacAgentComponent* Agent, EBehaviacStatus ChildStatus) override;
};
