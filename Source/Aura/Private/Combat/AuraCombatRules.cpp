// Copyright Druid Mechanics

#include "Combat/AuraCombatRules.h"

#include "AuraGameplayTags.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraCombatStateComponent.h"
#include "GameFramework/Actor.h"
#include "Engine/World.h"

namespace AuraCombatRulesPrivate
{
	constexpr uint32 TrustedPolicyCookie = 0xA3A00301u;

	bool IsAuthorityContext(const UObject* Context)
	{
		if (!IsValid(Context) || !Context->GetWorld())
		{
			return false;
		}

		if (Context->GetWorld()->GetNetMode() == NM_Client)
		{
			return false;
		}

		if (const AActor* ContextActor = Cast<AActor>(Context))
		{
			return ContextActor->HasAuthority();
		}

		return true;
	}

	bool IsKnownFaction(const FGameplayTag& Faction)
	{
		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		return Faction.MatchesTagExact(Tags.Faction_Player)
			|| Faction.MatchesTagExact(Tags.Faction_Enemy)
			|| Faction.MatchesTagExact(Tags.Faction_Civilian);
	}

	EAuraCombatRelationship GetRelationshipForFactions(const FGameplayTag& SourceFaction, const FGameplayTag& TargetFaction)
	{
		const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
		const bool bSourcePlayer = SourceFaction.MatchesTagExact(Tags.Faction_Player);
		const bool bSourceEnemy = SourceFaction.MatchesTagExact(Tags.Faction_Enemy);
		const bool bSourceCivilian = SourceFaction.MatchesTagExact(Tags.Faction_Civilian);
		const bool bTargetPlayer = TargetFaction.MatchesTagExact(Tags.Faction_Player);
		const bool bTargetEnemy = TargetFaction.MatchesTagExact(Tags.Faction_Enemy);
		const bool bTargetCivilian = TargetFaction.MatchesTagExact(Tags.Faction_Civilian);

		if ((bSourcePlayer && bTargetPlayer)
			|| (bSourceEnemy && bTargetEnemy)
			|| (bSourceCivilian && bTargetCivilian))
		{
			return EAuraCombatRelationship::Friendly;
		}

		if ((bSourcePlayer && bTargetEnemy) || (bSourceEnemy && bTargetPlayer))
		{
			return EAuraCombatRelationship::Hostile;
		}

		if ((bSourcePlayer || bSourceEnemy) && bTargetCivilian)
		{
			return EAuraCombatRelationship::Protected;
		}

		return EAuraCombatRelationship::Neutral;
	}

	EAuraCombatRuleRejectionReason GetStateReason(EAuraCombatLifeState State)
	{
		switch (State)
		{
		case EAuraCombatLifeState::Dying: return EAuraCombatRuleRejectionReason::Dying;
		case EAuraCombatLifeState::Dead: return EAuraCombatRuleRejectionReason::Dead;
		case EAuraCombatLifeState::Respawning: return EAuraCombatRuleRejectionReason::Respawning;
		case EAuraCombatLifeState::Alive: break;
		}
		return EAuraCombatRuleRejectionReason::None;
	}

	FAuraCombatRuleResult MakeInvalid(EAuraCombatRuleRejectionReason Reason)
	{
		FAuraCombatRuleResult Result;
		Result.RejectionReason = Reason;
		return Result;
	}
}

bool FAuraCombatPolicySnapshot::IsTrustedFor(const UObject* WorldContext) const
{
	return bValid
		&& TrustCookie == AuraCombatRulesPrivate::TrustedPolicyCookie
		&& TrustedWorld.IsValid()
		&& ::IsValid(WorldContext)
		&& WorldContext->GetWorld() == TrustedWorld.Get()
		&& AuraCombatRulesPrivate::IsAuthorityContext(WorldContext);
}

FAuraCombatRuleResult FAuraCombatRules::GetRelationship(
	const AActor* SourceActor,
	const AActor* TargetActor,
	const FAuraCombatRuleContext& Context)
{
	return Evaluate(SourceActor, TargetActor, Context, false, true);
}

FAuraCombatRuleResult FAuraCombatRules::CanCombatTarget(
	const AActor* SourceActor,
	const AActor* TargetActor,
	const FAuraCombatRuleContext& Context)
{
	return Evaluate(SourceActor, TargetActor, Context, false, false);
}

FAuraCombatRuleResult FAuraCombatRules::CanDamage(
	const AActor* SourceActor,
	const AActor* TargetActor,
	const FAuraCombatRuleContext& Context)
{
	return Evaluate(SourceActor, TargetActor, Context, true, false);
}

FAuraCombatRuleResult FAuraCombatRules::CanReceiveDamage(
	const AActor* TargetActor,
	const FAuraCombatRuleContext& Context)
{
	FAuraCombatRuleResult Result;
	if (!IsValid(TargetActor))
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidTarget);
	}

	const UAuraCombatIdentityComponent* IdentityComponent = UAuraCombatIdentityComponent::FindForActor(TargetActor);
	const UAuraCombatStateComponent* StateComponent = UAuraCombatStateComponent::FindForActor(TargetActor);
	if (!IdentityComponent || !IdentityComponent->HasValidIdentity() || !StateComponent)
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidTarget);
	}

	Result.Relationship = EAuraCombatRelationship::Neutral;
	if (!StateComponent->IsAlive())
	{
		Result.RejectionReason = AuraCombatRulesPrivate::GetStateReason(StateComponent->GetLifeState());
		return Result;
	}

	if (!IdentityComponent->GetIdentity().bCanBeDamaged)
	{
		Result.RejectionReason = EAuraCombatRuleRejectionReason::TargetNotDamageable;
		return Result;
	}

	Result.bCanDamage = true;
	Result.RejectionReason = EAuraCombatRuleRejectionReason::None;
	return Result;
}

FAuraCombatRuleResult FAuraCombatRules::Evaluate(
	const AActor* SourceActor,
	const AActor* TargetActor,
	const FAuraCombatRuleContext& Context,
	const bool bForDamage,
	const bool bRelationshipOnly)
{
	if (!IsValid(SourceActor))
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidSource);
	}
	if (!IsValid(TargetActor))
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidTarget);
	}

	const UAuraCombatIdentityComponent* SourceIdentityComponent = UAuraCombatIdentityComponent::FindForActor(SourceActor);
	const UAuraCombatIdentityComponent* TargetIdentityComponent = UAuraCombatIdentityComponent::FindForActor(TargetActor);
	if (!SourceIdentityComponent || !SourceIdentityComponent->HasValidIdentity()
		|| !AuraCombatRulesPrivate::IsKnownFaction(SourceIdentityComponent->GetIdentity().FactionTag))
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidSource);
	}
	if (!TargetIdentityComponent || !TargetIdentityComponent->HasValidIdentity()
		|| !AuraCombatRulesPrivate::IsKnownFaction(TargetIdentityComponent->GetIdentity().FactionTag))
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidTarget);
	}

	FAuraCombatRuleResult Result;
	Result.Relationship = AuraCombatRulesPrivate::GetRelationshipForFactions(
		SourceIdentityComponent->GetIdentity().FactionTag,
		TargetIdentityComponent->GetIdentity().FactionTag);

	const bool bSelfTarget = SourceActor == TargetActor;
	if (bSelfTarget && !Context.bExplicitSelfTargetIntent)
	{
		Result.RejectionReason = EAuraCombatRuleRejectionReason::SelfDenied;
		return Result;
	}

	if (bRelationshipOnly)
	{
		Result.RejectionReason = EAuraCombatRuleRejectionReason::None;
		return Result;
	}

	const UAuraCombatStateComponent* SourceStateComponent = UAuraCombatStateComponent::FindForActor(SourceActor);
	const UAuraCombatStateComponent* TargetStateComponent = UAuraCombatStateComponent::FindForActor(TargetActor);
	if (!SourceStateComponent)
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidSource);
	}
	if (!TargetStateComponent)
	{
		return AuraCombatRulesPrivate::MakeInvalid(EAuraCombatRuleRejectionReason::InvalidTarget);
	}
	if (!SourceStateComponent->IsAlive())
	{
		Result.RejectionReason = AuraCombatRulesPrivate::GetStateReason(SourceStateComponent->GetLifeState());
		return Result;
	}
	if (!TargetStateComponent->IsAlive())
	{
		Result.RejectionReason = AuraCombatRulesPrivate::GetStateReason(TargetStateComponent->GetLifeState());
		return Result;
	}

	const FAuraCombatIdentity& SourceIdentity = SourceIdentityComponent->GetIdentity();
	const FAuraCombatIdentity& TargetIdentity = TargetIdentityComponent->GetIdentity();
	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	const bool bSourceCivilian = SourceIdentity.FactionTag.MatchesTagExact(Tags.Faction_Civilian);
	if (bSourceCivilian)
	{
		Result.RejectionReason = EAuraCombatRuleRejectionReason::SourceCannotAttack;
		return Result;
	}
	if (!SourceIdentity.bCanAttack)
	{
		Result.RejectionReason = EAuraCombatRuleRejectionReason::SourceCannotAttack;
		return Result;
	}
	if (!TargetIdentity.bTargetable)
	{
		Result.RejectionReason = EAuraCombatRuleRejectionReason::TargetNotTargetable;
		return Result;
	}

	bool bRelationshipAllowed = false;
	if (bSelfTarget)
	{
		bRelationshipAllowed = true;
	}
	else
	{
		const bool bTrustedPolicy = Context.HasPolicySnapshot()
			&& Context.GetPolicySnapshot().IsTrustedFor(Context.TrustedWorldContext ? Context.TrustedWorldContext : SourceActor);

		switch (Result.Relationship)
		{
		case EAuraCombatRelationship::Hostile:
			bRelationshipAllowed = true;
			break;
		case EAuraCombatRelationship::Friendly:
			if (bTrustedPolicy && Context.GetPolicySnapshot().AllowsPvP() && SourceIdentity.bAllowFriendlyFire)
			{
				bRelationshipAllowed = true;
			}
			else
			{
				Result.RejectionReason = EAuraCombatRuleRejectionReason::Friendly;
			}
			break;
		case EAuraCombatRelationship::Protected:
			if (!bTrustedPolicy || Context.GetPolicySnapshot().IsTargetProtected())
			{
				Result.RejectionReason = bTrustedPolicy && Context.GetPolicySnapshot().IsTargetProtected()
					? EAuraCombatRuleRejectionReason::Protected
					: EAuraCombatRuleRejectionReason::PolicyDenied;
				break;
			}
			if (SourceIdentity.FactionTag.MatchesTagExact(Tags.Faction_Player)
				&& Context.GetPolicySnapshot().AllowsPlayerToCivilian())
			{
				bRelationshipAllowed = true;
			}
			else if (SourceIdentity.FactionTag.MatchesTagExact(Tags.Faction_Enemy)
				&& Context.GetPolicySnapshot().AllowsEnemyToCivilian())
			{
				bRelationshipAllowed = true;
			}
			else
			{
				Result.RejectionReason = EAuraCombatRuleRejectionReason::PolicyDenied;
			}
			break;
		case EAuraCombatRelationship::Neutral:
		default:
			Result.RejectionReason = EAuraCombatRuleRejectionReason::PolicyDenied;
			break;
		}
	}

	if (!bRelationshipAllowed)
	{
		return Result;
	}

	Result.bCanCombatTarget = true;
	if (bForDamage && !TargetIdentity.bCanBeDamaged)
	{
		Result.RejectionReason = EAuraCombatRuleRejectionReason::TargetNotDamageable;
		return Result;
	}

	Result.bCanDamage = bForDamage;
	Result.RejectionReason = EAuraCombatRuleRejectionReason::None;
	return Result;
}

#if WITH_DEV_AUTOMATION_TESTS
FAuraCombatPolicySnapshot FAuraCombatRules::MakeTrustedTestPolicySnapshot(
	const UObject* AuthorityWorldContext,
	const bool bAllowPvP,
	const bool bAllowPlayerToCivilian,
	const bool bAllowEnemyToCivilian,
	const bool bTargetProtected,
	const FName BattleZoneId,
	const FName BattleEventId)
{
	FAuraCombatPolicySnapshot Snapshot;
	if (!AuraCombatRulesPrivate::IsAuthorityContext(AuthorityWorldContext) || !AuthorityWorldContext->GetWorld())
	{
		return Snapshot;
	}

	Snapshot.bValid = true;
	Snapshot.bAllowPvP = bAllowPvP;
	Snapshot.bAllowPlayerToCivilian = bAllowPlayerToCivilian;
	Snapshot.bAllowEnemyToCivilian = bAllowEnemyToCivilian;
	Snapshot.bTargetProtected = bTargetProtected;
	Snapshot.BattleZoneId = BattleZoneId;
	Snapshot.BattleEventId = BattleEventId;
	Snapshot.TrustedWorld = AuthorityWorldContext->GetWorld();
	Snapshot.TrustCookie = AuraCombatRulesPrivate::TrustedPolicyCookie;
	return Snapshot;
}
#endif
