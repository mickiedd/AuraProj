// Copyright Druid Mechanics

#include "Tests/TestDataAbility.h"
#include "GameFramework/Actor.h"
#include "Abilities/GameplayAbilityTypes.h"

void UTestDataAbility::InitTestOwner(AActor* Owner)
{
    if (!Owner)
    {
        return;
    }
    // Set fields directly rather than InitFromActor (which asserts on a null ASC). With
    // no ASC, GetAbilityLevel() returns 1 (spec not found) and GetOwningActorFromActorInfo()
    // returns OwnerActor — exactly what the spawn nodes deref.
    ActorInfo.OwnerActor = Owner;
    ActorInfo.AvatarActor = Owner;
    SetCurrentActorInfo(FGameplayAbilitySpecHandle{}, &ActorInfo);
}