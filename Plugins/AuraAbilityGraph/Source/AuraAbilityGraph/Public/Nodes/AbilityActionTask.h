// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "AbilityGraphTypes.h"
#include "AbilityActionTask.generated.h"

class UAuraAbilityActionNode;
class UAuraDataAbility;

UCLASS(Abstract)
class AURAABILITYGRAPH_API UAuraAbilityActionTask : public UObject
{
    GENERATED_BODY()

public:
    // UPROPERTY so the task graph is properly GC-rooted. Raw pointers here would let
    // GC collect a child task (or scramble its vtable to 0xFFFFFFFFFFFFFFFF during PIE
    // teardown) before SequenceTask::Cancel walks ChildTasks and calls the virtual
    // Cancel — the teardown access violation seen in the crash dumps.
    UPROPERTY()
    UAuraAbilityActionNode* NodeDef = nullptr;

    UPROPERTY()
    UAuraDataAbility* OwnerAbility = nullptr;

    UPROPERTY()
    UAuraAbilityActionTask* ParentTask = nullptr;

    UPROPERTY()
    TArray<UAuraAbilityActionTask*> ChildTasks;

    int32 ActiveChildIndex = 0;

    virtual void Init(UAuraAbilityActionNode* InNode, UAuraDataAbility* InOwner);
    EAuraAbilityActionStatus Execute(FAuraAbilityExecutionContext& Ctx);
    virtual void OnEnter(FAuraAbilityExecutionContext& Ctx) {}
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) { return EAuraAbilityActionStatus::Success; }
    virtual EAuraAbilityActionStatus OnTick(FAuraAbilityExecutionContext& Ctx) { return EAuraAbilityActionStatus::Running; }
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) {}
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx) {}

    bool HasEntered = false;
    EAuraAbilityActionStatus PendingStatus = EAuraAbilityActionStatus::Invalid;
};
