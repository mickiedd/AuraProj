// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Interaction/AuraInteractionTypes.h"

class AActor;

/** Immutable native Day 14 interaction table composed with current server state. */
class AURA_API FAuraInteractionPolicy
{
public:
	static bool Resolve(const AActor* Requester, const AActor* Target, FGameplayTag OptionTag,
		FAuraResolvedInteractionPolicy& OutPolicy, EAuraInteractionResultCode& OutFailure);
};
