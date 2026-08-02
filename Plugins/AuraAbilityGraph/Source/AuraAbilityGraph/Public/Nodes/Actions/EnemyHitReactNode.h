// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "EnemyHitReactNode.generated.h"

class UAbilityTask_PlayMontageAndWait;

UCLASS(DisplayName = "EnemyHitReact")
class AURAABILITYGRAPH_API UEnemyHitReactNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
};

UCLASS()
class AURAABILITYGRAPH_API UEnemyHitReactTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx) override;

private:
    UFUNCTION()
    void OnCompleted();

    UFUNCTION()
    void OnInterrupted();

    void Finish(EAuraAbilityActionStatus Status);
    void RemoveHitReactTag();

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilitySystemComponent> ASC;

    FGameplayTag HitReactTag;
    bool bTagAdded = false;
    bool bFinishing = false;
};
