// Copyright Druid Mechanics

#include "Nodes/Actions/SpawnProjectilesNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Actor/AuraProjectile.h"
#include "AuraGameplayTags.h"
#include "Interaction/CombatInterface.h"
#include "AuraAbilityGraphLogChannels.h"

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
        else if (Property.Name == TEXT("TargetFromContext"))
        {
            TargetFromContext = Property.Value;
        }
    }
}

EAuraAbilityActionStatus USpawnProjectilesTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnProjectiles] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectiles] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }

    const USpawnProjectilesNode* Node = Cast<USpawnProjectilesNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectiles] OnStart abort: NodeDef is not USpawnProjectilesNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnProjectiles] OnStart SocketTag=%s SocketLoc=%s"), *Node->SocketTag.ToString(), *SocketLocation.ToString());
    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnProjectiles] OnStart Target=%s Actor=%s"), *TargetLocation.ToString(), *GetNameSafe(Ctx.CursorHit.GetActor()));
    if (TargetLocation.IsZero())
    {
        TargetLocation = Ctx.AvatarActor->GetActorLocation() + Ctx.AvatarActor->GetActorForwardVector() * 1000.f;
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnProjectiles] OnStart using fallback target=%s"), *TargetLocation.ToString());
    }

    FRotator Rotation = (TargetLocation - SocketLocation).Rotation();
    const FVector Forward = Rotation.Vector();
    const int32 EffectiveCount = FMath::Max(1, Node->Count);
    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnProjectiles] OnStart Count=%d EffectiveCount=%d Spread=%.1f"), Node->Count, EffectiveCount, Node->Spread);
    TArray<FRotator> Rotations = UAuraAbilitySystemLibrary::EvenlySpacedRotators(Forward, FVector::UpVector, Node->Spread, EffectiveCount);

    TSubclassOf<AAuraProjectile> ProjectileClass = LoadClass<AAuraProjectile>(nullptr, *Node->ProjectileClass);
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

        if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
        {
            if (const UAuraAbilityDefinition* Definition = DataAbility->GetDefinition())
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
                Params.KnockbackForce = FVector::UpVector * Definition->KnockbackForceMagnitude;
                Params.KnockbackChance = Definition->KnockbackChance;
                Params.bIsRadialDamage = false;
                Params.RadialDamageInnerRadius = 0.f;
                Params.RadialDamageOuterRadius = 0.f;
                Params.RadialDamageOrigin = FVector::ZeroVector;
                Projectile->DamageEffectParams = Params;
            }
        }

        Projectile->FinishSpawning(SpawnTransform);
        UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnProjectiles] OnStart spawned projectile[%d] class=%s"), i, *ProjectileClass->GetName());
    }

    UE_LOG(LogAuraAbilityGraph, Log, TEXT("[SpawnProjectiles] OnStart spawned %d projectiles"), Rotations.Num());
    return EAuraAbilityActionStatus::Success;
}
