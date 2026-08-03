// Copyright Druid Mechanics

#include "Nodes/Actions/MulticastGunFXNode.h"
#include "Nodes/AbilityActionNode.h"
#include "Nodes/AbilityActionTask.h"
#include "DataAbility.h"
#include "Character/AuraCharacterBase.h"
#include "Interaction/CombatInterface.h"
#include "Particles/ParticleSystem.h"
#include "Sound/SoundBase.h"
#include "AuraAbilityGraphLogChannels.h"
#include "Kismet/GameplayStatics.h"

UAuraAbilityActionTask* UMulticastGunFXNode::CreateTask(UObject* Outer) const
{
    return NewObject<UMulticastGunFXTask>(Outer);
}

void UMulticastGunFXNode::LoadFromProperties(int32 Version, const TArray<FAuraAbilityGraphProperty>& Properties)
{
    Super::LoadFromProperties(Version, Properties);
    for (const FAuraAbilityGraphProperty& Property : Properties)
    {
        if (Property.Name == TEXT("MuzzleSocketTag"))
        {
            MuzzleSocketTag = FGameplayTag::RequestGameplayTag(FName(*Property.Value), false);
        }
        else if (Property.Name == TEXT("MuzzleEffect"))
        {
            MuzzleEffect = Property.Value;
            LoadedMuzzleEffect = LoadObject<UParticleSystem>(nullptr, *Property.Value);
        }
        else if (Property.Name == TEXT("FireSound"))
        {
            FireSound = Property.Value;
            LoadedFireSound = LoadObject<USoundBase>(nullptr, *Property.Value);
        }
    }
}

EAuraAbilityActionStatus UMulticastGunFXTask::OnStart(FAuraAbilityExecutionContext& Ctx)
{
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[MulticastGunFX] OnStart OwnerAbility=%s Avatar=%s"), *GetNameSafe(OwnerAbility), *GetNameSafe(Ctx.AvatarActor));
    if (!OwnerAbility || !Ctx.AvatarActor)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[MulticastGunFX] OnStart abort: missing OwnerAbility or AvatarActor"));
        return EAuraAbilityActionStatus::Failure;
    }

    const UMulticastGunFXNode* Node = Cast<UMulticastGunFXNode>(NodeDef);
    if (!Node)
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[MulticastGunFX] OnStart abort: NodeDef is not UMulticastGunFXNode"));
        return EAuraAbilityActionStatus::Failure;
    }

    const FVector MuzzleLocation = Ctx.AvatarActor->Implements<UCombatInterface>()
        ? ICombatInterface::Execute_GetCombatSocketLocation(Ctx.AvatarActor, Node->MuzzleSocketTag)
        : Ctx.AvatarActor->GetActorLocation();
    UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[MulticastGunFX] OnStart MuzzleSocketTag=%s MuzzleLoc=%s"), *Node->MuzzleSocketTag.ToString(), *MuzzleLocation.ToString());

    if (AAuraCharacterBase* AuraCharacter = Cast<AAuraCharacterBase>(Ctx.AvatarActor))
    {
        AuraCharacter->MulticastPlayGunFireFX(MuzzleLocation, Node->LoadedMuzzleEffect, Node->LoadedFireSound);
        UE_LOG(LogAuraAbilityGraph, Verbose, TEXT("[MulticastGunFX] OnStart multicast FX sent"));
    }
    else
    {
        UE_LOG(LogAuraAbilityGraph, Warning, TEXT("[MulticastGunFX] OnStart using generic local FX fallback for AvatarActor=%s"), *GetNameSafe(Ctx.AvatarActor));
        if (UWorld* World = Ctx.AvatarActor->GetWorld())
        {
            if (Node->LoadedMuzzleEffect)
            {
                UGameplayStatics::SpawnEmitterAtLocation(World, Node->LoadedMuzzleEffect, MuzzleLocation);
            }
            if (Node->LoadedFireSound)
            {
                UGameplayStatics::PlaySoundAtLocation(World, Node->LoadedFireSound, MuzzleLocation);
            }
        }
    }

    return EAuraAbilityActionStatus::Success;
}
