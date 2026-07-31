// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "DataAbility.h"
#include "TestDataAbility.generated.h"

class AActor;

/**
 * Test-only subclass of UAuraDataAbility used to drive spawn nodes headlessly without a
 * real GAS activation. The spawn nodes deref two non-virtual UGameplayAbility methods on
 * the owner ability:
 *   - GetOwningActorFromActorInfo()  -> reads CurrentActorInfo->OwnerActor (used as the
 *     SpawnActor Owner/Instigator). Passes its ensure only when the ability is
 *     instantiated, so we keep UAuraDataAbility's InstancedPerActor policy.
 *   - GetAbilityLevel()              -> returns 1 gracefully when the ASC/spec is absent.
 *
 * InitTestOwner() populates CurrentActorInfo (a protected member of UGameplayAbility,
 * reachable from this subclass) via SetCurrentActorInfo, pointing at the test avatar.
 * No ability spec / ASC / attributes are required.
 */
UCLASS()
class AURAABILITYGRAPH_API UTestDataAbility : public UAuraDataAbility
{
    GENERATED_BODY()

public:
    // Point the ability's actor info at the test avatar so GetOwningActorFromActorInfo()
    // returns it and GetAbilityLevel() returns 1. The ActorInfo member must outlive the
    // ability's use of it, so it is held on this object.
    void InitTestOwner(AActor* Owner);

private:
    FGameplayAbilityActorInfo ActorInfo;
};