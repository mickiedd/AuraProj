// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "HitscanTraceNode.generated.h"

UCLASS(DisplayName = "HitscanTrace")
class AURAABILITYGRAPH_API UHitscanTraceNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitscanTrace")
    FGameplayTag SocketTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitscanTrace")
    float TraceRange = 10000.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "HitscanTrace")
    float ScatterRadius = 50.f;
};

UCLASS()
class AURAABILITYGRAPH_API UHitscanTraceTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};
