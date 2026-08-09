// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AuraCombatTypes.generated.h"

UENUM(BlueprintType)
enum class EAuraCombatLifeState : uint8
{
	Alive,
	Dying,
	Dead,
	Respawning
};

UENUM(BlueprintType)
enum class EAuraCombatQueryPurpose : uint8
{
	CombatTargeting,
	Damage
};

UENUM(BlueprintType)
enum class EAuraCombatRelationship : uint8
{
	Friendly,
	Hostile,
	Neutral,
	Protected
};

UENUM(BlueprintType)
enum class EAuraCombatRuleRejectionReason : uint8
{
	None,
	InvalidSource,
	InvalidTarget,
	SelfDenied,
	SourceCannotAttack,
	SourceNotAlive,
	TargetNotTargetable,
	TargetNotDamageable,
	Friendly,
	Protected,
	PolicyDenied,
	Dying,
	Dead,
	Respawning
};

/**
 * Server-owned relationship policy. The fields intentionally have no public
 * mutators; a valid permissive snapshot can only be produced by an
 * authoritative resolver or the development-only test factory.
 */
USTRUCT(BlueprintType)
struct AURA_API FAuraCombatPolicySnapshot
{
	GENERATED_BODY()

	bool IsValid() const { return bValid; }
	bool IsTrustedFor(const UObject* WorldContext) const;

	bool AllowsPvP() const { return bAllowPvP; }
	bool AllowsPlayerToCivilian() const { return bAllowPlayerToCivilian; }
	bool AllowsEnemyToCivilian() const { return bAllowEnemyToCivilian; }
	bool IsTargetProtected() const { return bTargetProtected; }
	FName GetBattleZoneId() const { return BattleZoneId; }
	FName GetBattleEventId() const { return BattleEventId; }

private:
	friend class FAuraCombatRules;

	bool bValid = false;
	bool bAllowPvP = false;
	bool bAllowPlayerToCivilian = false;
	bool bAllowEnemyToCivilian = false;
	bool bTargetProtected = true;
	FName BattleZoneId = NAME_None;
	FName BattleEventId = NAME_None;
	TWeakObjectPtr<UWorld> TrustedWorld;
	uint32 TrustCookie = 0;
};

USTRUCT(BlueprintType)
struct AURA_API FAuraCombatRuleResult
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat Rules")
	EAuraCombatRelationship Relationship = EAuraCombatRelationship::Neutral;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Rules")
	bool bCanCombatTarget = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Rules")
	bool bCanDamage = false;

	UPROPERTY(BlueprintReadOnly, Category = "Combat Rules")
	EAuraCombatRuleRejectionReason RejectionReason = EAuraCombatRuleRejectionReason::InvalidSource;

	bool IsAllowed() const { return bCanCombatTarget || bCanDamage; }
};

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
