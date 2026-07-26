// Copyright Druid Mechanics

#include "Nodes/Actions/SpawnProjectileNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "Actor/AuraProjectile.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "Interaction/CombatInterface.h"
#include "Engine/EngineTypes.h"
#include "AuraAbilityGraphLogChannels.h"

UAuraAbilityActionTask* USpawnProjectileNode::CreateTask(UObject* Outer) const
{
    return NewObject<USpawnProjectileTask>(Outer);
}

void USpawnProjectileNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
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
        else if (Property.Name == TEXT("TargetFromContext"))
        {
            TargetFromContext = Property.Value;
        }
    }
}

EAuraAbilityActionStatus USpawnProjectileTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectile] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }

    const USpawnProjectileNode* Node = Cast<USpawnProjectileNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectile] OnStart abort: NodeDef is not USpawnProjectileNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->SocketTag);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart SocketTag=%s SocketLoc=%s"), *Node->SocketTag.ToString(), *SocketLocation.ToString());
    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart TargetLocation=%s TargetActor=%s"), *TargetLocation.ToString(), *GetNameSafe(Ctx.CursorHit.GetActor()));
    if (TargetLocation.IsZero())
    {
        TargetLocation = Ctx.AvatarActor->GetActorLocation() + Ctx.AvatarActor->GetActorForwardVector() * 1000.f;
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart using fallback target=%s"), *TargetLocation.ToString());
    }

    FRotator Rotation = (TargetLocation - SocketLocation).Rotation();
    FTransform SpawnTransform;
    SpawnTransform.SetLocation(SocketLocation);
    SpawnTransform.SetRotation(Rotation.Quaternion());

    TSubclassOf<AAuraProjectile> ProjectileClass = LoadClass<AAuraProjectile>(nullptr, *Node->ProjectileClass);
    if (!ProjectileClass)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectile] OnStart LoadClass failed for '%s', trying StaticLoadClass"), *Node->ProjectileClass);
        ProjectileClass = StaticLoadClass(AAuraProjectile::StaticClass(), nullptr, *Node->ProjectileClass);
    }
    if (!ProjectileClass)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectile] OnStart abort: failed to load ProjectileClass='%s'"), *Node->ProjectileClass);
        return EAuraAbilityActionStatus::Failure;
    }

    AAuraProjectile* Projectile = Ctx.AvatarActor->GetWorld()->SpawnActorDeferred<AAuraProjectile>(
        ProjectileClass,
        SpawnTransform,
        OwnerAbility->GetOwningActorFromActorInfo(),
        Cast<APawn>(OwnerAbility->GetOwningActorFromActorInfo()),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
        ESpawnActorScaleMethod::MultiplyWithRoot);

    if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        if (const UAuraAbilityDefinition* Definition = Ctx.Definition)
        {
            const FVector Direction = (TargetLocation - SocketLocation).GetSafeNormal();
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
            Params.DeathImpulse = Direction * Definition->DeathImpulseMagnitude;
            Params.KnockbackForceMagnitude = Definition->KnockbackForceMagnitude;
            Params.KnockbackForce = FVector::UpVector * Definition->KnockbackForceMagnitude;
            Params.KnockbackChance = Definition->KnockbackChance;
            Params.bIsRadialDamage = false;
            Params.RadialDamageInnerRadius = 0.f;
            Params.RadialDamageOuterRadius = 0.f;
            Params.RadialDamageOrigin = FVector::ZeroVector;
            Projectile->DamageEffectParams = Params;
            UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart damage base=%.1f type=%s"), Params.BaseDamage, *Params.DamageType.ToString());
        }
    }

    Projectile->FinishSpawning(SpawnTransform);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart projectile spawned class=%s"), *ProjectileClass->GetName());
    return EAuraAbilityActionStatus::Success;
}

void USpawnProjectileTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
}
