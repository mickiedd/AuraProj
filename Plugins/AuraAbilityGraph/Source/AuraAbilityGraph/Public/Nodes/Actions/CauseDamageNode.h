// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "CauseDamageNode.generated.h"

UCLASS(DisplayName = "CauseDamage")
class AURAABILITYGRAPH_API UCauseDamageNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "CauseDamage")
    FString TargetFromContext;
};

UCLASS()
class AURAABILITYGRAPH_API UCauseDamageTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};
