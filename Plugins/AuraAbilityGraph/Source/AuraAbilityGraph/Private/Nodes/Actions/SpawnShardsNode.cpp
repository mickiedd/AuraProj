// Copyright Druid Mechanics

#include "Nodes/Actions/SpawnShardsNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Actor/PointCollection.h"
#include "AuraAbilityTypes.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Engine/World.h"
#include "TimerManager.h"
#include "GameplayCueManager.h"
#include "GameplayTagContainer.h"

UAuraAbilityActionTask* USpawnShardsNode::CreateTask(UObject* Outer) const
{
    return NewObject<USpawnShardsTask>(Outer);
}

void USpawnShardsNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("PointCollectionClass"))
        {
            PointCollectionClass = Property.Value;
        }
        else if (Property.Name == TEXT("ShardClass"))
        {
            ShardClass = Property.Value;
        }
        else if (Property.Name == TEXT("MaxShards"))
        {
            MaxShards = FCString::Atoi(*Property.Value);
        }
        else if (Property.Name == TEXT("SpawnInterval"))
        {
            SpawnInterval = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("RadialDamageRadius"))
        {
            RadialDamageRadius = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("GameplayCueTag"))
        {
            GameplayCueTag = Property.Value;
        }
    }
}

EAuraAbilityActionStatus USpawnShardsTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnShards] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));

    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnShards] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }

    // Only run on server
    if (!Ctx.AvatarActor->HasAuthority())
    {
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnShards] OnStart skipped on non-authority"));
        return EAuraAbilityActionStatus::Success;
    }

    const USpawnShardsNode* Node = Cast<USpawnShardsNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnShards] OnStart abort: NodeDef is not USpawnShardsNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    CachedCtx = Ctx;

    // Determine target location from cursor hit
    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    if (TargetLocation.IsZero())
    {
        TargetLocation = Ctx.AvatarActor->GetActorLocation() + Ctx.AvatarActor->GetActorForwardVector() * 1000.f;
    }

    // Spawn PointCollection actor at target location
    TSubclassOf<APointCollection> PCClass = LoadClass<APointCollection>(nullptr, *Node->PointCollectionClass);
    if (!PCClass)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnShards] OnStart abort: failed to load PointCollectionClass '%s'"), *Node->PointCollectionClass);
        return EAuraAbilityActionStatus::Failure;
    }

    FTransform SpawnTransform;
    SpawnTransform.SetLocation(TargetLocation);
    PointCollectionActor = Ctx.AvatarActor->GetWorld()->SpawnActor<APointCollection>(
        PCClass, SpawnTransform, FActorSpawnParameters());

    if (!PointCollectionActor.IsValid())
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnShards] OnStart abort: failed to spawn PointCollection actor"));
        return EAuraAbilityActionStatus::Failure;
    }

    // Get ground points — clamp to MaxShards
    const int32 NumShards = FMath::Min(Node->MaxShards, FMath::Max(1, OwnerAbility->GetAbilityLevel()));
    APointCollection* PCActor = Cast<APointCollection>(PointCollectionActor.Get());
    if (!PCActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnShards] OnStart abort: PointCollection actor is not an APointCollection"));
        return EAuraAbilityActionStatus::Failure;
    }
    TArray<USceneComponent*> GroundPoints = PCActor->GetGroundPoints(TargetLocation, NumShards);

    // Extract world locations
    ShardLocations.Empty();
    for (USceneComponent* Pt : GroundPoints)
    {
        ShardLocations.Add(Pt->GetComponentLocation());
    }

    CurrentShardIndex = 0;

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnShards] OnStart spawned PointCollection at %s, %d ground points, starting timer interval=%.2f"),
        *TargetLocation.ToCompactString(), ShardLocations.Num(), Node->SpawnInterval);

    // Start timer to spawn shards one at a time
    UWorld* World = Ctx.AvatarActor->GetWorld();
    if (!World || ShardLocations.Num() == 0)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnShards] OnStart abort: no world or no shard locations"));
        return EAuraAbilityActionStatus::Failure;
    }

    // Spawn first shard immediately, then set timer for the rest
    SpawnNextShard();

    if (CurrentShardIndex < ShardLocations.Num())
    {
        FTimerDelegate TimerDel;
        TimerDel.BindWeakLambda(OwnerAbility, [ThisObj = TStrongObjectPtr<USpawnShardsTask>(this)]()
        {
            if (!ThisObj->OwnerAbility)
            {
                return;
            }
            ThisObj->SpawnNextShard();

            if (ThisObj->CurrentShardIndex >= ThisObj->ShardLocations.Num())
            {
                // All shards spawned — advance the graph
                ThisObj->PendingStatus = EAuraAbilityActionStatus::Success;
                if (UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(ThisObj->OwnerAbility))
                {
                    DataAbility->AdvanceGraph(EAuraAbilityActionStatus::Success);
                }
            }
        });

        World->GetTimerManager().SetTimer(SpawnTimerHandle, TimerDel, Node->SpawnInterval, true);
        return EAuraAbilityActionStatus::Running;
    }

    // Only one shard (or none left) — done immediately
    return EAuraAbilityActionStatus::Success;
}

void USpawnShardsTask::SpawnNextShard()
{
    if (CurrentShardIndex >= ShardLocations.Num())
    {
        return;
    }

    const USpawnShardsNode* Node = Cast<USpawnShardsNode>(NodeDef);
    if (!Node || !OwnerAbility)
    {
        return;
    }

    const FVector& ShardLocation = ShardLocations[CurrentShardIndex];
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnShards] Spawning shard %d/%d at %s"),
        CurrentShardIndex + 1, ShardLocations.Num(), *ShardLocation.ToCompactString());

    // Execute gameplay cue if specified
    if (!Node->GameplayCueTag.IsEmpty())
    {
        FGameplayCueParameters CueParams;
        CueParams.Location = ShardLocation;
        if (CachedCtx.AvatarActor)
        {
            UGameplayCueManager::ExecuteGameplayCue_NonReplicated(
                CachedCtx.AvatarActor,
                FGameplayTag::RequestGameplayTag(FName(*Node->GameplayCueTag), false),
                CueParams);
        }
    }

    // Deal radial damage to nearby players
    if (CachedCtx.AvatarActor && CachedCtx.Definition)
    {
        TArray<AActor*> ActorsToIgnore;
        ActorsToIgnore.Add(CachedCtx.AvatarActor);

        TArray<AActor*> OverlappingActors;
        UAuraAbilitySystemLibrary::GetLivePlayersWithinRadius(
            CachedCtx.AvatarActor,
            OverlappingActors,
            ActorsToIgnore,
            Node->RadialDamageRadius,
            ShardLocation);

        for (AActor* TargetActor : OverlappingActors)
        {
            UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(TargetActor);
            if (!TargetASC)
            {
                continue;
            }

            if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
            {
                const FVector Direction = (TargetActor->GetActorLocation() - ShardLocation).GetSafeNormal();
                FDamageEffectParams Params;
                Params.SourceAbilitySystemComponent = CachedCtx.ASC;
                Params.TargetAbilitySystemComponent = TargetASC;
                Params.AbilityLevel = DataAbility->GetAbilityLevel();
                Params.DamageGameplayEffectClass = CachedCtx.Definition->DamageEffectClass;
                Params.DamageType = CachedCtx.Definition->DamageType;
                Params.BaseDamage = CachedCtx.Definition->Damage.GetValueAtLevel(DataAbility->GetAbilityLevel());
                Params.DebuffChance = CachedCtx.Definition->DebuffChance;
                Params.DebuffDamage = CachedCtx.Definition->DebuffDamage;
                Params.DebuffDuration = CachedCtx.Definition->DebuffDuration;
                Params.DebuffFrequency = CachedCtx.Definition->DebuffFrequency;
                Params.DeathImpulseMagnitude = CachedCtx.Definition->DeathImpulseMagnitude;
                Params.DeathImpulse = Direction * CachedCtx.Definition->DeathImpulseMagnitude;
                Params.KnockbackForceMagnitude = CachedCtx.Definition->KnockbackForceMagnitude;
                Params.KnockbackForce = Direction * CachedCtx.Definition->KnockbackForceMagnitude;
                Params.KnockbackChance = CachedCtx.Definition->KnockbackChance;
                Params.bIsRadialDamage = true;
                Params.RadialDamageOrigin = ShardLocation;
                Params.RadialDamageInnerRadius = 0.f;
                Params.RadialDamageOuterRadius = Node->RadialDamageRadius;
                UAuraAbilitySystemLibrary::ApplyDamageEffect(Params);
            }
        }
    }

    CurrentShardIndex++;
}

void USpawnShardsTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnShards] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));

    if (UWorld* World = OwnerAbility ? OwnerAbility->GetWorld() : nullptr)
    {
        World->GetTimerManager().ClearTimer(SpawnTimerHandle);
    }

    // Clean up the PointCollection actor
    if (PointCollectionActor.IsValid())
    {
        PointCollectionActor->Destroy();
        PointCollectionActor.Reset();
    }

    ShardLocations.Empty();
    CurrentShardIndex = 0;
}