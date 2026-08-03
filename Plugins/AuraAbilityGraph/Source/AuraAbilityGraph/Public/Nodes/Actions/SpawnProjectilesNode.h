// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "SpawnProjectilesNode.generated.h"

UCLASS(DisplayName = "SpawnProjectiles")
class AURAABILITYGRAPH_API USpawnProjectilesNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    FGameplayTag SocketTag;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    FString ProjectileClass;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    FName ProjectileDefinition;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    int32 Count = 5;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    float Spread = 90.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    bool bHoming = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    float HomingAccelerationMin = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    float HomingAccelerationMax = 0.f;

    /** When true, sets ReturnToActor = avatar on spawned AAuraFireBall actors so they fly back. */
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnProjectiles")
    bool bSetReturnToOwner = false;
};

UCLASS()
class AURAABILITYGRAPH_API USpawnProjectilesTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
};
