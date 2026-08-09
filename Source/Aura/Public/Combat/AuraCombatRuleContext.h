// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Combat/AuraCombatTypes.h"
#include "AuraCombatRuleContext.generated.h"

/**
 * Inputs shared by combat-targeting and damage-rule queries. Actor references
 * are supplied by the caller for query convenience, but permissive policy
 * data is accepted only when its server provenance matches the context world.
 */
USTRUCT(BlueprintType)
struct AURA_API FAuraCombatRuleContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadWrite, Category = "Combat Rules")
	EAuraCombatQueryPurpose QueryPurpose = EAuraCombatQueryPurpose::CombatTargeting;

	/** Trusted only as a world lookup anchor; authority is checked by the rules. */
	const UObject* TrustedWorldContext = nullptr;
	const AActor* SourceActor = nullptr;
	const AActor* TargetActor = nullptr;

	UPROPERTY(BlueprintReadWrite, Category = "Combat Rules")
	FVector ImpactLocation = FVector::ZeroVector;

	UPROPERTY(BlueprintReadWrite, Category = "Combat Rules")
	FName BattleZoneId = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "Combat Rules")
	FName BattleEventId = NAME_None;

	UPROPERTY(BlueprintReadWrite, Category = "Combat Rules")
	bool bExplicitSelfTargetIntent = false;

	void SetPolicySnapshot(const FAuraCombatPolicySnapshot& InSnapshot)
	{
		PolicySnapshot = InSnapshot;
		bHasPolicySnapshot = true;
	}

	bool HasPolicySnapshot() const { return bHasPolicySnapshot; }
	const FAuraCombatPolicySnapshot& GetPolicySnapshot() const { return PolicySnapshot; }

private:
	FAuraCombatPolicySnapshot PolicySnapshot;
	bool bHasPolicySnapshot = false;
};
