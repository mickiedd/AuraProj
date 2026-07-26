// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "SpawnProjectileNode.generated.h"

UCLASS(DisplayName = "SpawnProjectile")
class AURAABILITYGRAPH_API USpawnProjectileNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectile")
    FGameplayTag SocketTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectile")
    FString ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectile")
    FString TargetFromContext;
};

UCLASS()
class AURAABILITYGRAPH_API USpawnProjectileTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;
};
