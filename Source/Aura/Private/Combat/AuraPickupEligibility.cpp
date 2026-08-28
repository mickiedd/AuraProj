// Copyright Druid Mechanics

#include "Combat/AuraPickupEligibility.h"

#include "AuraGameplayTags.h"
#include "Combat/AuraCombatIdentityComponent.h"

bool FAuraPickupEligibility::CanReceive(const AActor* TargetActor, const bool bAllowEnemies)
{
	if (!IsValid(TargetActor))
	{
		return false;
	}

	const UAuraCombatIdentityComponent* IdentityComponent = UAuraCombatIdentityComponent::FindForActor(TargetActor);
	if (!IdentityComponent || !IdentityComponent->HasValidIdentity())
	{
		return false;
	}

	const FGameplayTag Faction = IdentityComponent->GetIdentity().FactionTag;
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	if (Faction.MatchesTagExact(Tags.Faction_Player))
	{
		return true;
	}
	if (Faction.MatchesTagExact(Tags.Faction_Enemy))
	{
		return bAllowEnemies;
	}

	// Civilians and unknown factions are deliberately not pickup recipients;
	// civilian identity must never inherit enemy loot/fire-area behavior.
	return false;
}
