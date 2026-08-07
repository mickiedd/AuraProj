// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AuraCombatTypes.generated.h"

/**
 * Replicated, server-owned identity used by combat relationship and targeting code.
 * Categorical values are explicit native gameplay tags; behavior flags are never
 * inferred from a faction or profile.
 */
USTRUCT(BlueprintType)
struct AURA_API FAuraCombatIdentity
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	FGameplayTag FactionTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	FGameplayTag ControlTypeTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	FGameplayTag CombatProfileTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	FGameplayTag DeathPolicyTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	bool bTargetable = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	bool bCanAttack = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	bool bCanBeDamaged = false;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Combat Identity")
	bool bAllowFriendlyFire = false;

	bool IsValid() const
	{
		return FactionTag.IsValid()
			&& ControlTypeTag.IsValid()
			&& CombatProfileTag.IsValid()
			&& DeathPolicyTag.IsValid();
	}

	bool operator==(const FAuraCombatIdentity& Other) const
	{
		return FactionTag.MatchesTagExact(Other.FactionTag)
			&& ControlTypeTag.MatchesTagExact(Other.ControlTypeTag)
			&& CombatProfileTag.MatchesTagExact(Other.CombatProfileTag)
			&& DeathPolicyTag.MatchesTagExact(Other.DeathPolicyTag)
			&& bTargetable == Other.bTargetable
			&& bCanAttack == Other.bCanAttack
			&& bCanBeDamaged == Other.bCanBeDamaged
			&& bAllowFriendlyFire == Other.bAllowFriendlyFire;
	}

	bool operator!=(const FAuraCombatIdentity& Other) const
	{
		return !(*this == Other);
	}
};
