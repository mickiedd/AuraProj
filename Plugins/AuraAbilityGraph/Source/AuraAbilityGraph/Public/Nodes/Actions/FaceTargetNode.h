// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "FaceTargetNode.generated.h"

UCLASS(DisplayName = "FaceTarget")
class AURAABILITYGRAPH_API UFaceTargetNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceTarget")
    bool bSetControllerRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceTarget")
    bool bSetActorRotation = true;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "FaceTarget")
    bool bYawOnly = false;
};

UCLASS()
class AURAABILITYGRAPH_API UFaceTargetTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};