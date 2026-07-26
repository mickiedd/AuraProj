// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "WaitForTargetDataNode.generated.h"

class UTargetDataUnderMouse;

UCLASS(DisplayName = "WaitForTargetData")
class AURAABILITYGRAPH_API UWaitForTargetDataNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class AURAABILITYGRAPH_API UWaitForTargetDataTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;

private:
    UFUNCTION()
    void OnValidData(const FGameplayAbilityTargetDataHandle& DataHandle);

    TWeakObjectPtr<UTargetDataUnderMouse> TargetDataTask;
};
