// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "WaitForMontageEventNode.generated.h"

UCLASS(DisplayName = "WaitForMontageEvent")
class AURAABILITYGRAPH_API UWaitForMontageEventNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "WaitForMontageEvent")
    FGameplayTag EventTag;
};

UCLASS()
class AURAABILITYGRAPH_API UWaitForMontageEventTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;

private:
    UFUNCTION()
    void OnEventReceived(FGameplayEventData EventData);
};
