// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "EnemyCombatMontageNode.generated.h"

class UAbilityTask_PlayMontageAndWait;
class UAbilityTask_WaitGameplayEvent;

/** Selects an authored attack montage from ICombatInterface and waits for its gameplay event. */
UCLASS(DisplayName = "EnemyCombatMontage")
class AURAABILITYGRAPH_API UEnemyCombatMontageNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyCombatMontage")
    float Timeout = 5.f;
};

UCLASS()
class AURAABILITYGRAPH_API UEnemyCombatMontageTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;
    virtual void Cancel(FAuraAbilityExecutionContext& Ctx) override;

private:
    UFUNCTION()
    void OnEventReceived(FGameplayEventData EventData);

    UFUNCTION()
    void OnMontageInterrupted();

    void OnTimeout();
    void Cleanup();

    UPROPERTY()
    TObjectPtr<UAbilityTask_PlayMontageAndWait> MontageTask;

    UPROPERTY()
    TObjectPtr<UAbilityTask_WaitGameplayEvent> EventTask;

    FTimerHandle TimeoutHandle;
};
