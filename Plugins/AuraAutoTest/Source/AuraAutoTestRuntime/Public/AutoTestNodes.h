// Aura AutoTest Plugin
// Licensed under the BSD 3-Clause License.

#pragma once

#include "CoreMinimal.h"
#include "BehaviorTree/BehaviorUBehaviorNode.h"
#include "BehaviorTree/BehaviorUBehaviorTask.h"
#include "AutoTestNodes.generated.h"

class UBehaviorUAgentComponent;

// ===================================================================
// Comparison operator shared by Assert / Expect / WaitForProperty.
// String values are compared numerically when both sides parse as numbers,
// otherwise by string equality (Equals/NotEquals only).
// ===================================================================

UENUM(BlueprintType)
enum class EAutoTestCompareOp : uint8
{
	Equals			UMETA(DisplayName = "Equals"),
	NotEquals		UMETA(DisplayName = "NotEquals"),
	GreaterThan		UMETA(DisplayName = "GreaterThan"),
	LessThan		UMETA(DisplayName = "LessThan"),
	GreaterEqual		UMETA(DisplayName = "GreaterEqual"),
	LessEqual		UMETA(DisplayName = "LessEqual"),
};

// ===================================================================
// Assert — fail the test immediately if the condition is false.
// Worker-thread safe: reads the blackboard only.
// XML: <Assert Key="Self.Health" Op="GreaterThan" Value="0" Message="..."/>
// ===================================================================

UCLASS(DisplayName = "Assert")
class AURAAUTOTESTRUNTIME_API UAutoTestAssertNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Op; // EAutoTestCompareOp name

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Value; // literal or Self.X

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Message;
};

UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestAssertTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
};

// ===================================================================
// Expect — record a soft assertion without terminating the test.
// Returns Success either way unless Fatal="true" (then behaves like Assert).
// XML: <Expect Key="..." Op="..." Value="..." Message="..." Fatal="false"/>
// ===================================================================

UCLASS(DisplayName = "Expect")
class AURAAUTOTESTRUNTIME_API UAutoTestExpectNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Op;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	bool bFatal = false;
};

UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestExpectTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
};

// ===================================================================
// WaitForProperty — return Running until the condition holds, then Success.
// Fails the test on timeout if FailOnTimeout is true.
// XML: <WaitForProperty Key="Self.Ready" Op="Equals" Value="true"
//                       Timeout="5.0" FailOnTimeout="true" Message="..."/>
// ===================================================================

UCLASS(DisplayName = "WaitForProperty")
class AURAAUTOTESTRUNTIME_API UAutoTestWaitForPropertyNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Key;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Op;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Value;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	float Timeout = 0.0f; // 0 = no timeout (wait forever)

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	bool bFailOnTimeout = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Message;
};

UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestWaitForPropertyTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
public:
	virtual void Reset(UBehaviorUAgentComponent* Agent) override;
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
private:
	double StartTime = 0.0;
	bool bStarted = false;
};

// ===================================================================
// SpawnActor — defer an actor spawn to the game thread via the method
// command queue (Phase 2 worker thread cannot spawn). Stashes config into
// fixed blackboard keys; the agent's "SpawnActor" method handler reads them,
// spawns on the game thread, and stores the actor into the OutputKey blackboard
// object property. Returns Running until the command result arrives (1 frame).
// XML: <SpawnActor Class="Self.ClassPath" Location="Self.Loc"
//                  OutputKey="SpawnedActor" Method="SpawnActor"/>
// ===================================================================

UCLASS(DisplayName = "SpawnActor")
class AURAAUTOTESTRUNTIME_API UAutoTestSpawnActorNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString ClassPath; // e.g. /Game/.../BP_Enemy.BP_Enemy or /Script/Aura.AuraEnemy

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Location; // literal vector or Self.X

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString OutputKey; // blackboard object-property key to store the spawned actor

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString MethodName = TEXT("SpawnActor"); // method handler name on the test agent
};

UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestSpawnActorTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
public:
	virtual void Reset(UBehaviorUAgentComponent* Agent) override;
protected:
	virtual EBehaviorUStatus OnUpdate(UBehaviorUAgentComponent* Agent, EBehaviorUStatus ChildStatus) override;
	virtual void OnExit(UBehaviorUAgentComponent* Agent, EBehaviorUStatus InStatus) override;
private:
	uint64 PendingCommandId = 0;
	bool bHasPendingCommand = false;
};

// ===================================================================
// LogResult — record a checkpoint assertion + emit to LogAuraTest.
// Returns Success; does not affect tree flow.
// XML: <LogResult Message="Reached checkpoint 3" Passed="true"/>
// ===================================================================

UCLASS(DisplayName = "LogResult")
class AURAAUTOTESTRUNTIME_API UAutoTestLogResultNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Message;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	bool bPassed = true;
};

UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestLogResultTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
};

// ===================================================================
// Pass / Fail — mark the test terminal. The runner observes terminal status
// and stops the tree.
// XML: <Pass/> or <Fail Message="..."/>
// ===================================================================

UCLASS(DisplayName = "Pass")
class AURAAUTOTESTRUNTIME_API UAutoTestPassNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestPassTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
};

UCLASS(DisplayName = "Fail")
class AURAAUTOTESTRUNTIME_API UAutoTestFailNode : public UBehaviorUBehaviorNode
{
	GENERATED_BODY()
public:
	virtual UBehaviorUBehaviorTask* CreateTask(UObject* Outer) const override;
	virtual void LoadFromProperties(int32 Version, const FString& InAgentType, const TArray<FBehaviorUProperty>& Properties) override;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "AutoTest")
	FString Message;
};

UCLASS()
class AURAAUTOTESTRUNTIME_API UAutoTestFailTask : public UBehaviorULeafTask
{
	GENERATED_BODY()
protected:
	virtual bool OnEnter(UBehaviorUAgentComponent* Agent) override;
};