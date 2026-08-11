// Copyright Druid Mechanics

#include "Tests/TestCombatAvatar.h"
#include "Components/SphereComponent.h"
#include "AuraAbilityGraphLogChannels.h"

ATestCombatAvatar::ATestCombatAvatar()
{
    PrimaryActorTick.bCanEverTick = false;
    // Spawned into a transient (non-networked) game world -> ROLE_Authority, so the
    // spawn nodes' HasAuthority() gates (SpawnShards, ElectrocuteBeam) pass.
    bReplicates = false;

    CollisionSphere = CreateDefaultSubobject<USphereComponent>("CollisionSphere");
    RootComponent = CollisionSphere;
    CollisionSphere->InitSphereRadius(SphereRadius);
    // Visible to the beam's Visibility-channel sphere trace; ignore everything else so
    // it doesn't interfere with projectile spawn/overlap logic.
    CollisionSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    CollisionSphere->SetCollisionObjectType(ECollisionChannel::ECC_WorldDynamic);
    CollisionSphere->SetCollisionResponseToAllChannels(ECollisionResponse::ECR_Ignore);
    CollisionSphere->SetCollisionResponseToChannel(ECollisionChannel::ECC_Visibility, ECollisionResponse::ECR_Block);
    CollisionSphere->SetGenerateOverlapEvents(false);
}

FVector ATestCombatAvatar::GetCombatSocketLocation_Implementation(const FGameplayTag& MontageTag)
{
    return GetActorLocation() + SocketOffset;
}

bool ATestCombatAvatar::IsDead_Implementation() const
{
    return bTestDead;
}

AActor* ATestCombatAvatar::GetAvatar_Implementation()
{
    return this;
}
