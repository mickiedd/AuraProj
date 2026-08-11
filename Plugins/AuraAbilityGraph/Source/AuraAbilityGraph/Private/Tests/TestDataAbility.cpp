// Copyright Druid Mechanics

#include "Tests/TestDataAbility.h"
#include "AbilitySystemComponent.h"
#include "GameFramework/Actor.h"
#include "Abilities/GameplayAbilityTypes.h"

void UTestDataAbility::InitTestOwner(AActor* Owner, int32 AbilityLevel)
{
    if (!Owner)
    {
        return;
    }
    // Set fields directly rather than InitFromActor (which asserts on a null ASC).
    ActorInfo.OwnerActor = Owner;
    ActorInfo.AvatarActor = Owner;

    if (AbilityLevel > 1)
    {
        TestAbilitySystemComponent = NewObject<UAbilitySystemComponent>(Owner);
        if (TestAbilitySystemComponent)
        {
            TestAbilitySystemComponent->RegisterComponent();
            TestAbilitySystemComponent->InitAbilityActorInfo(Owner, Owner);
            const FGameplayAbilitySpec Spec(GetClass(), AbilityLevel);
            const FGameplayAbilitySpecHandle Handle = TestAbilitySystemComponent->GiveAbility(Spec);
            ActorInfo.AbilitySystemComponent = TestAbilitySystemComponent;
            SetCurrentActorInfo(Handle, &ActorInfo);
            return;
        }
    }

    // With no ASC/spec, GetAbilityLevel() returns 1 and the spawn nodes still
    // have the owner/avatar fields they dereference.
    SetCurrentActorInfo(FGameplayAbilitySpecHandle{}, &ActorInfo);
}
