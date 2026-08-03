// Copyright Druid Mechanics

#include "Nodes/Actions/SpawnProjectilesNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Actor/AuraProjectile.h"
#include "Actor/AuraFireBall.h"
#include "Interaction/CombatInterface.h"
#include "AuraAbilityGraphLogChannels.h"
#include "GameFramework/ProjectileMovementComponent.h"
#include "Data/AuraGameplayConfig.h"

UAuraAbilityActionTask* USpawnProjectilesNode::CreateTask(UObject* Outer) const
{
    return NewObject<USpawnProjectilesTask>(Outer);
}

void USpawnProjectilesNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("SocketTag"))
        {
            SocketTag = FGameplayTag::RequestGameplayTag(FName(*Property.Value), false);
        }
        else if (Property.Name == TEXT("ProjectileClass"))
        {
            ProjectileClass = Property.Value;
        }
        else if (Property.Name == TEXT("ProjectileDefinition"))
        {
            ProjectileDefinition = FName(*Property.Value);
        }
        else if (Property.Name == TEXT("Count"))
        {
            Count = FCString::Atoi(*Property.Value);
        }
        else if (Property.Name == TEXT("Spread"))
        {
            Spread = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("bHoming"))
        {
            bHoming = Property.Value.ToBool();
        }
        else if (Property.Name == TEXT("HomingAccelerationMin"))
        {
            HomingAccelerationMin = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("HomingAccelerationMax"))
        {
            HomingAccelerationMax = FCString::Atof(*Property.Value);
        }
        else if (Property.Name == TEXT("bSetReturnToOwner"))
        {
            bSetReturnToOwner = Property.Value.ToBool();
        }
    }
}

EAuraAbilityActionStatus USpawnProjectilesTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectiles] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectiles] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }
    if (!Ctx.AvatarActor->HasAuthority()) return EAuraAbilityActionStatus::Success;

    const USpawnProjectilesNode* Node = Cast<USpawnProjectilesNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectiles] OnStart abort: NodeDef is not USpawnProjectilesNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectiles] OnStart SocketTag=%s SocketLoc=%s"), *Node->SocketTag.ToString(), *SocketLocation.ToString());
    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectiles] OnStart Target=%s Actor=%s"), *TargetLocation.ToString(), *GetNameSafe(Ctx.CursorHit.GetActor()));
    if (TargetLocation.IsZero())
    {
        TargetLocation = Ctx.AvatarActor->GetActorLocation() + Ctx.AvatarActor->GetActorForwardVector() * 1000.f;
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectiles] OnStart using fallback target=%s"), *TargetLocation.ToString());
    }

    FRotator Rotation = (TargetLocation - SocketLocation).Rotation();
    const FVector Forward = Rotation.Vector();
    const int32 EffectiveCount = FMath::Max(1, Node->Count);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectiles] OnStart Count=%d EffectiveCount=%d Spread=%.1f"), Node->Count, EffectiveCount, Node->Spread);
    TArray<FRotator> Rotations = UAuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, Node->Spread, EffectiveCount);

    const FAuraProjectileDefinition* ProjectileDefinition = Node->ProjectileDefinition.IsNone() ? nullptr : FAuraGameplayConfig::FindProjectile(Node->ProjectileDefinition);
    TSubclassOf<AAuraProjectile> ProjectileClass;
    if (ProjectileDefinition) ProjectileClass = ProjectileDefinition->NativeClass;
    else ProjectileClass = LoadClass<AAuraProjectile>(nullptr, *Node->ProjectileClass);
    if (!ProjectileClass)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectiles] OnStart LoadClass failed for '%s', trying StaticLoadClass"), *Node->ProjectileClass);
        ProjectileClass = StaticLoadClass(AAuraProjectile::StaticClass(), nullptr, *Node->ProjectileClass);
    }
    if (!ProjectileClass)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectiles] OnStart abort: failed to load ProjectileClass='%s'"), *Node->ProjectileClass);
        return EAuraAbilityActionStatus::Failure;
    }

    for (int32 i = 0; i < Rotations.Num(); ++i)
    {
        const FRotator& Rot = Rotations[i];
        FTransform SpawnTransform;
        SpawnTransform.SetLocation(SocketLocation);
        SpawnTransform.SetRotation(Rot.Quaternion());

        AAuraProjectile* Projectile = Ctx.AvatarActor->GetWorld()->SpawnActorDeferred<AAuraProjectile>(
            ProjectileClass,
            SpawnTransform,
            OwnerAbility->GetOwningActorFromActorInfo(),
            Cast<APawn>(OwnerAbility->GetOwningActorFromActorInfo()),
            ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
            ESpawnActorScaleMethod::MultiplyWithRoot);
        if (!Projectile || (!Node->ProjectileDefinition.IsNone() && !Projectile->ConfigureFromDefinition(Node->ProjectileDefinition)))
        {
            if (Projectile) Projectile->Destroy();
            UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SpawnProjectiles] Invalid ProjectileDefinition='%s'"), *Node->ProjectileDefinition.ToString());
            return EAuraAbilityActionStatus::Failure;
        }

        if (Node->bSetReturnToOwner)
        {
            if (AAuraFireBall* FireBall = Cast<AAuraFireBall>(Projectile))
            {
                FireBall->ReturnToActor = Ctx.AvatarActor;
                FireBall->SetOwner(Ctx.AvatarActor);
            }
        }

        if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
        {
            if (const UAuraAbilityDefinition* Definition = Ctx.Definition)
            {
                FDamageEffectParams Params;
                Params.SourceAbilitySystemComponent = Ctx.ASC;
                Params.TargetAbilitySystemComponent = nullptr;
                Params.AbilityLevel = DataAbility->GetAbilityLevel();
                Params.DamageGameplayEffectClass = Definition->DamageEffectClass;
                Params.DamageType = Definition->DamageType;
                Params.BaseDamage = Definition->Damage.GetValueAtLevel(DataAbility->GetAbilityLevel());
                Params.DebuffChance = Definition->DebuffChance;
                Params.DebuffDamage = Definition->DebuffDamage;
                Params.DebuffDuration = Definition->DebuffDuration;
                Params.DebuffFrequency = Definition->DebuffFrequency;
                Params.DeathImpulseMagnitude = Definition->DeathImpulseMagnitude;
                Params.DeathImpulse = Forward * Definition->DeathImpulseMagnitude;
                Params.KnockbackForceMagnitude = Definition->KnockbackForceMagnitude;
                Params.KnockbackForce = Forward * Definition->KnockbackForceMagnitude;
                Params.KnockbackChance = Definition->KnockbackChance;
                Params.bIsRadialDamage = false;
                Params.RadialDamageInnerRadius = 0.f;
                Params.RadialDamageOuterRadius = 0.f;
                Params.RadialDamageOrigin = FVector::ZeroVector;
                Projectile->DamageEffectParams = Params;

                if (Node->bHoming)
                {
                    AActor* HomingTarget = Ctx.CursorHit.GetActor();
                    if (HomingTarget && HomingTarget->Implements<UCombatInterface>())
                    {
                        Projectile->ProjectileMovement->HomingTargetComponent = HomingTarget->GetRootComponent();
                    }
                    else
                    {
                        Projectile->HomingTargetSceneComponent = NewObject<USceneComponent>(Projectile, TEXT("HomingTargetSceneComponent"));
                        Projectile->HomingTargetSceneComponent->SetWorldLocation(TargetLocation);
                        Projectile->ProjectileMovement->HomingTargetComponent = Projectile->HomingTargetSceneComponent;
                    }
                    Projectile->ProjectileMovement->HomingAccelerationMagnitude = FMath::FRandRange(Node->HomingAccelerationMin, Node->HomingAccelerationMax);
                    Projectile->ProjectileMovement->bIsHomingProjectile = true;
                }
            }
        }

        Projectile->FinishSpawning(SpawnTransform);

        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectiles] OnStart spawned projectile[%d] class=%s"), i, *ProjectileClass->GetName());
    }

    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectiles] OnStart spawned %d projectiles"), Rotations.Num());
    return EAuraAbilityActionStatus::Success;
}
