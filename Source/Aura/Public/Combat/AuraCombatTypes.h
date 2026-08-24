// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "AuraCombatTypes.generated.h"

class AActor;

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

USTRUCT(BlueprintType)
struct AURA_API FAuraFatalDamageContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	TObjectPtr<AActor> VictimActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FVector DeathImpulse = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FGameplayTag DamageType;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FName BattleZoneId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FName BattleEventId = NAME_None;

	bool IsValid() const { return ::IsValid(SourceActor) || ::IsValid(VictimActor); }
};

USTRUCT(BlueprintType)
struct AURA_API FAuraDeathEvent
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	TObjectPtr<AActor> SourceActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	TObjectPtr<AActor> VictimActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FGameplayTag DeathPolicyTag;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FGameplayTag DamageType;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FVector DeathImpulse = FVector::ZeroVector;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FName BattleZoneId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	FName BattleEventId = NAME_None;

	UPROPERTY(BlueprintReadOnly, Category = "Combat|Death")
	int32 DeathSequence = 0;

	bool IsValid() const { return ::IsValid(VictimActor) && DeathSequence > 0 && DeathPolicyTag.IsValid(); }
};

/**
 * Replicated non-combat half of an applied role. Concrete merchant, job, and
 * offer identities intentionally do not belong here; those are population data.
 */
USTRUCT(BlueprintType)
struct AURA_API FAuraAppliedRoleState
{
	GENERATED_BODY()

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Applied Role")
	FName RoleId = NAME_None;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Applied Role")
	FGameplayTag EntityTypeTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Applied Role")
	FGameplayTag EconomyProfileTag;

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Applied Role")
	FGameplayTag InteractionProfileTag;

	bool IsValid() const
	{
		return !RoleId.IsNone()
			&& EntityTypeTag.IsValid()
			&& EconomyProfileTag.IsValid()
			&& InteractionProfileTag.IsValid();
	}

	bool operator==(const FAuraAppliedRoleState& Other) const
	{
		return RoleId == Other.RoleId
			&& EntityTypeTag.MatchesTagExact(Other.EntityTypeTag)
			&& EconomyProfileTag.MatchesTagExact(Other.EconomyProfileTag)
			&& InteractionProfileTag.MatchesTagExact(Other.InteractionProfileTag);
	}

	bool operator!=(const FAuraAppliedRoleState& Other) const
	{
		return !(*this == Other);
	}
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
		return FactionTag == Other.FactionTag
			&& ControlTypeTag == Other.ControlTypeTag
			&& CombatProfileTag == Other.CombatProfileTag
			&& DeathPolicyTag == Other.DeathPolicyTag
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
