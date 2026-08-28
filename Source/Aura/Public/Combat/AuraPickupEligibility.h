// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

class AActor;

/** Explicit identity-based pickup eligibility policy. */
class AURA_API FAuraPickupEligibility
{
public:
	/** Players are eligible; enemies require explicit opt-in; civilians and unknown identities reject. */
	static bool CanReceive(const AActor* TargetActor, bool bAllowEnemies);
};
