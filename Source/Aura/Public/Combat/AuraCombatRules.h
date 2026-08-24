// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"
#include "Combat/AuraCombatRuleContext.h"

/** Centralized Day 3 combat relationship and permission rules. */
class AURA_API FAuraCombatRules
{
public:
	static FAuraCombatRuleResult GetRelationship(
		const AActor* SourceActor,
		const AActor* TargetActor,
		const FAuraCombatRuleContext& Context = FAuraCombatRuleContext());

	static FAuraCombatRuleResult CanCombatTarget(
		const AActor* SourceActor,
		const AActor* TargetActor,
		const FAuraCombatRuleContext& Context = FAuraCombatRuleContext());

	static FAuraCombatRuleResult CanDamage(
		const AActor* SourceActor,
		const AActor* TargetActor,
		const FAuraCombatRuleContext& Context = FAuraCombatRuleContext());

	static FAuraCombatRuleResult CanReceiveDamage(
		const AActor* TargetActor,
		const FAuraCombatRuleContext& Context = FAuraCombatRuleContext());

	/** Builds a server-trusted snapshot for a resolved battle-zone policy. */
	static FAuraCombatPolicySnapshot MakeAuthoritativePolicySnapshot(
		const UObject* AuthorityWorldContext,
		bool bAllowPvP,
		bool bAllowPlayerToCivilian,
		bool bAllowEnemyToCivilian,
		bool bTargetProtected,
		FName BattleZoneId,
		FName BattleEventId);

#if WITH_DEV_AUTOMATION_TESTS
	/**
	 * Creates a permissive snapshot only for authority-side automation fixtures.
	 * The returned value is invalid when called with a client or non-authority
	 * world context.
	 */
	static FAuraCombatPolicySnapshot MakeTrustedTestPolicySnapshot(
		const UObject* AuthorityWorldContext,
		bool bAllowPvP = false,
		bool bAllowPlayerToCivilian = false,
		bool bAllowEnemyToCivilian = false,
		bool bTargetProtected = false,
		FName BattleZoneId = NAME_None,
		FName BattleEventId = NAME_None);
#endif

private:
	static FAuraCombatRuleResult Evaluate(
		const AActor* SourceActor,
		const AActor* TargetActor,
		const FAuraCombatRuleContext& Context,
		bool bForDamage,
		bool bRelationshipOnly);
};
