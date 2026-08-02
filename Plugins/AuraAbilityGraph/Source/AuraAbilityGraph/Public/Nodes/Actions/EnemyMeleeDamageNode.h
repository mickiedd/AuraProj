// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "EnemyMeleeDamageNode.generated.h"

/** Applies the definition's damage once to each hostile living actor around a combat socket. */
UCLASS(DisplayName = "EnemyMeleeDamage")
class AURAABILITYGRAPH_API UEnemyMeleeDamageNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "EnemyMeleeDamage")
    float Radius = 45.f;
};

UCLASS()
class AURAABILITYGRAPH_API UEnemyMeleeDamageTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};
