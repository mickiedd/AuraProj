// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "SpawnShardsNode.generated.h"

/**
 * SpawnShards node — encapsulates the ArcaneShards ability mechanic.
 *
 * Spawns a BP_PointCollection actor at the cursor hit location, samples ground
 * points, then spawns shard actors one at a time on a timer. Each shard deals
 * radial damage to nearby players using ApplyDamageEffect.
 *
 * XML properties:
 *   PointCollectionClass  — BP path to the PointCollection actor (default: BP_PointCollection)
 *   ShardClass            — BP path to the shard actor (default: BP_MagicCircle)
 *   MaxShards             — max number of shards to spawn (default: 11)
 *   SpawnInterval         — seconds between shard spawns (default: 0.1)
 *   RadialDamageRadius    — radius for radial damage around each shard (default: 300)
 *   GameplayCueTag        — gameplay cue tag to execute on each shard spawn
 */
UCLASS(DisplayName = "SpawnShards")
class AURAABILITYGRAPH_API USpawnShardsNode : public UAuraAbilityActionNode
{
    GENERATED_BODY()

public:
    virtual UAuraAbilityActionTask* CreateTask(UObject* Outer) const override;
    virtual void LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties) override;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnShards")
    FString PointCollectionClass = TEXT("/Game/Blueprints/AbilitySystem/Aura/Abilities/Arcane/ArcaneShards/BP_PointCollection.BP_PointCollection_C");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnShards")
    FString ShardClass = TEXT("/Game/Blueprints/AbilitySystem/Aura/Abilities/Arcane/ArcaneShards/BP_MagicCircle.BP_MagicCircle_C");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnShards")
    int32 MaxShards = 11;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnShards")
    float SpawnInterval = 0.1f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnShards")
    float RadialDamageRadius = 300.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "SpawnShards")
    FString GameplayCueTag;
};

UCLASS()
class AURAABILITYGRAPH_API USpawnShardsTask : public UAuraAbilityActionTask
{
    GENERATED_BODY()

public:
    virtual EAuraAbilityActionStatus OnStart(FAuraAbilityExecutionContext& Ctx) override;
    virtual void OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status) override;

private:
    FTimerHandle SpawnTimerHandle;
    TWeakObjectPtr<AActor> PointCollectionActor;
    TArray<FVector> ShardLocations;
    int32 CurrentShardIndex = 0;
    FAuraAbilityExecutionContext CachedCtx;

    void SpawnNextShard();
};