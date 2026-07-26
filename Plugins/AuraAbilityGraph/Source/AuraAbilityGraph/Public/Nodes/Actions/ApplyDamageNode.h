// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "ApplyDamageNode.generated.h"

UCLASS(DisplayName = "ApplyDamage")
class AURAABILITYGRAPH_API UApplyDamageNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "ApplyDamage")
    FString TargetFromContext;
};

UCLASS()
class AURAABILITYGRAPH_API UApplyDamageTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};
