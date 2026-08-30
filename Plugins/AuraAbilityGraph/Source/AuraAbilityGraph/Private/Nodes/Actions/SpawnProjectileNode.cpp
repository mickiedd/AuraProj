// Copyright Druid Mechanics

#include "Nodes/Actions/SpawnProjectileNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "AbilityDefinition.h"
#include "DataAbility.h"
#include "Actor/AuraProjectile.h"
#include "AbilitySystem/AuraAbilitySystemLibrary.h"
#include "AbilitySystemBlueprintLibrary.h"
#include "Interaction/CombatInterface.h"
#include "Engine/EngineTypes.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Data/AuraGameplayConfig.h"
#include "AbilityGraphTypes.h"

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
        else if (Property.Name == TEXT("ProjectileDefinition"))
        {
            ProjectileDefinition = FName(*Property.Value);
        }
        else if (Property.Name == TEXT("SocketFromContext"))
        {
            bSocketFromContext = Property.Value.ToBool();
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
    if (!Ctx.AvatarActor->HasAuthority()) return EAuraAbilityActionStatus::Success;

    const USpawnProjectileNode* Node = Cast<USpawnProjectileNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[SpawnProjectile] OnStart abort: NodeDef is not USpawnProjectileNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    const FGameplayTag EffectiveSocketTag = Node->bSocketFromContext && Ctx.CombatSocketTag.IsValid()
        ? Ctx.CombatSocketTag : Node->SocketTag;
    const FVector SocketLocation = ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, EffectiveSocketTag);
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart SocketTag=%s SocketLoc=%s"), *Node->SocketTag.ToString(), *SocketLocation.ToString());
    FVector TargetLocation = Ctx.CursorHit.ImpactPoint;
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart TargetLocation=%s TargetActor=%s"), *TargetLocation.ToString(), *GetNameSafe(Ctx.CursorHit.GetActor()));
    if (TargetLocation.IsZero())
    {
        TargetLocation = Ctx.AvatarActor->GetActorLocation() + Ctx.AvatarActor->GetActorForwardVector() * 1000.f;
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart using fallback target=%s"), *TargetLocation.ToString());
    }
    UAbilitySystemComponent* TargetASC = UAbilitySystemBlueprintLibrary::GetAbilitySystemComponent(Ctx.CursorHit.GetActor());

    FRotator Rotation = (TargetLocation - SocketLocation).Rotation();
    FTransform SpawnTransform;
    SpawnTransform.SetLocation(SocketLocation);
    SpawnTransform.SetRotation(Rotation.Quaternion());

    const FAuraProjectileDefinition* ProjectileDefinition = Node->ProjectileDefinition.IsNone() ? nullptr : FAuraGameplayConfig::FindProjectile(Node->ProjectileDefinition);
    TSubclassOf<AAuraProjectile> ProjectileClass;
    if (ProjectileDefinition) ProjectileClass = ProjectileDefinition->NativeClass;
    else ProjectileClass = LoadClass<AAuraProjectile>(nullptr, *Node->ProjectileClass);
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
        Ctx.AvatarActor,
        Cast<APawn>(Ctx.AvatarActor),
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn,
        ESpawnActorScaleMethod::MultiplyWithRoot);
    if (!Projectile || (!Node->ProjectileDefinition.IsNone() && !Projectile->ConfigureFromDefinition(Node->ProjectileDefinition)))
    {
        if (Projectile) Projectile->Destroy();
        UE_LOG(LogAuraAbilityGraph, Error, TEXT("[SpawnProjectile] Invalid ProjectileDefinition='%s'"), *Node->ProjectileDefinition.ToString());
        return EAuraAbilityActionStatus::Failure;
    }

    if (const UAuraDataAbility* DataAbility = Cast<UAuraDataAbility>(OwnerAbility))
    {
        if (const UAuraAbilityDefinition* Definition = Ctx.Definition)
        {
            const FVector Direction = (TargetLocation - SocketLocation).GetSafeNormal();
            FDamageEffectParams Params;
            Definition->BuildDamageEffectParams(Params, Ctx.ASC, TargetASC, Ctx.AvatarActor, DataAbility->GetAbilityLevel(), Direction);
            Projectile->DamageEffectParams = Params;
            UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart damage base=%.1f type=%s"), Params.BaseDamage, *Params.DamageType.ToString());
        }
    }

    IAuraFirearmAuthority* FirearmAuthority = nullptr;
    if (Ctx.Definition && Ctx.Definition->IsFirearmDefinition())
    {
        UObject* AbilityOwner = Ctx.ASC ? Ctx.ASC->GetOwner() : nullptr;
        if (!AbilityOwner || !AbilityOwner->GetClass()->ImplementsInterface(UAuraFirearmAuthority::StaticClass()))
        {
            Projectile->Destroy();
            UE_LOG(LogAuraAbilityGraph, Error, TEXT("[Firearm][Server] Shot rejected ability=%s result=AuthorityUnavailable"),
                *Ctx.Definition->AbilityName.ToString());
            return EAuraAbilityActionStatus::Failure;
        }
        FirearmAuthority = Cast<IAuraFirearmAuthority>(AbilityOwner);
        FName ResultCode = NAME_None;
        if (!FirearmAuthority || !FirearmAuthority->TryConsumeFirearmRound(Ctx.Definition->AbilityName, Ctx.AvatarActor, ResultCode))
        {
            Projectile->Destroy();
            UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[Firearm][Server] Shot rejected ability=%s result=%s"),
                *Ctx.Definition->AbilityName.ToString(), *ResultCode.ToString());
            return EAuraAbilityActionStatus::Failure;
        }
    }

    Projectile->FinishSpawning(SpawnTransform);
    if (FirearmAuthority) FirearmAuthority->NotifyFirearmShotAccepted(Ctx.Definition->AbilityName);
    if (UObject* AbilityOwner = Ctx.ASC ? Ctx.ASC->GetOwner() : nullptr;
        AbilityOwner && AbilityOwner->GetClass()->ImplementsInterface(UAuraAbilityCommitAuthority::StaticClass()))
    {
        if (IAuraAbilityCommitAuthority* CommitAuthority = Cast<IAuraAbilityCommitAuthority>(AbilityOwner))
        {
            CommitAuthority->NotifyAuthoritativeAbilityCommitted(Ctx.Definition ? Ctx.Definition->AbilityName : NAME_None);
        }
    }
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnStart projectile spawned class=%s"), *ProjectileClass->GetName());
    return EAuraAbilityActionStatus::Success;
}

void USpawnProjectileTask::OnExit(FAuraAbilityExecutionContext& Ctx, EAuraAbilityActionStatus Status)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[SpawnProjectile] OnExit status=%s"), *StaticEnum<EAuraAbilityActionStatus>()->GetValueAsString(Status));
}
