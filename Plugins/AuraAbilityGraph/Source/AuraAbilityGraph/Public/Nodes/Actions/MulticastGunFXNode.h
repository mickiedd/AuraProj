// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "MulticastGunFXNode.generated.h"

UCLASS(DisplayName = "MulticastGunFX")
class AURAABILITYGRAPH_API UMulticastGunFXNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MulticastGunFX")
    FGameplayTag MuzzleSocketTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MulticastGunFX")
    FString MuzzleEffect;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "MulticastGunFX")
    FString FireSound;
};

UCLASS()
class AURAABILITYGRAPH_API UMulticastGunFXTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};
