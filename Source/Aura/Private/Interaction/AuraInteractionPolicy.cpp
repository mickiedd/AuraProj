// Copyright Druid Mechanics

#include "Interaction/AuraInteractionPolicy.h"

#include "AuraGameplayTags.h"
#include "Battle/AuraBattleDirector.h"
#include "Battle/AuraBattleZoneConfig.h"
#include "Character/AuraCharacterBase.h"
#include "Combat/AuraCombatIdentityComponent.h"
#include "Combat/AuraTargetableInterface.h"
#include "Combat/AuraCombatStateComponent.h"
#include "Game/AuraGameModeBase.h"
#include "GameFramework/Actor.h"

bool FAuraInteractionPolicy::Resolve(const AActor* Requester, const AActor* Target, FGameplayTag OptionTag,
	FAuraResolvedInteractionPolicy& OutPolicy, EAuraInteractionResultCode& OutFailure)
{
	OutPolicy = FAuraResolvedInteractionPolicy();
	OutFailure = EAuraInteractionResultCode::OptionUnavailable;
	if (!IsValid(Requester) || !IsValid(Target) || Requester->GetWorld() != Target->GetWorld() || !OptionTag.IsValid())
	{
		OutFailure = !IsValid(Target) ? EAuraInteractionResultCode::InvalidTarget : EAuraInteractionResultCode::InvalidRequester;
		return false;
	}
	const AAuraCharacterBase* RequesterCharacter = Cast<AAuraCharacterBase>(Requester);
	const UAuraCombatIdentityComponent* RequesterIdentity = UAuraCombatIdentityComponent::FindForActor(Requester);
	if (!RequesterCharacter || !RequesterIdentity || !RequesterIdentity->HasValidIdentity()
		|| !RequesterIdentity->GetIdentity().FactionTag.MatchesTagExact(FAuraGameplayTags::Get().Faction_Player)
		|| !RequesterIdentity->GetIdentity().bTargetable || !RequesterCharacter->IsCombatAlive()
		|| !RequesterCharacter->GetAppliedRoleState().IsValid()
		|| !RequesterCharacter->GetAppliedRoleState().InteractionProfileTag.MatchesTagExact(FAuraGameplayTags::Get().Interaction_Combatant))
	{
		OutFailure = EAuraInteractionResultCode::InvalidRequester;
		return false;
	}
	const IAuraTargetableInterface* Targetable = Cast<IAuraTargetableInterface>(Target);
	if (!Targetable) { OutFailure = EAuraInteractionResultCode::InvalidTarget; return false; }

	TArray<FAuraInteractionOption> IntrinsicOptions;
	Targetable->GetAuraInteractionOptions(Requester, IntrinsicOptions);
	const FAuraInteractionOption* Intrinsic = IntrinsicOptions.FindByPredicate(
		[OptionTag](const FAuraInteractionOption& Option) { return Option.OptionTag.MatchesTagExact(OptionTag); });
	if (!Intrinsic) return false;
	if (!Intrinsic->bEnabled) { OutFailure = Intrinsic->DisabledReason; return false; }

	const FAuraGameplayTags& Tags = FAuraGameplayTags::Get();
	OutPolicy.OptionTag = OptionTag;
	OutPolicy.bRequiresLineOfSight = true;
	OutPolicy.AllowedLifeStates.Add(EAuraCombatLifeState::Alive);
	if (OptionTag.MatchesTagExact(Tags.Interaction_Talk))
	{
		OutPolicy.HandlerId = TEXT("Talk"); OutPolicy.MaxRangeCm = 250.f;
		OutPolicy.AllowedTargetKinds.Add(Tags.Target_Kind_Civilian);
		OutPolicy.AllowedPhases = {EAuraBattlePhase::Peace, EAuraBattlePhase::Alert};
	}
	else if (OptionTag.MatchesTagExact(Tags.Interaction_Observe))
	{
		OutPolicy.HandlerId = TEXT("Observe"); OutPolicy.MaxRangeCm = 600.f;
		OutPolicy.AllowedTargetKinds.Add(Tags.Target_Kind_Civilian);
		OutPolicy.AllowedPhases = {EAuraBattlePhase::Peace, EAuraBattlePhase::Alert, EAuraBattlePhase::Conflict, EAuraBattlePhase::Cleanup};
	}
	else if (OptionTag.MatchesTagExact(Tags.Interaction_UseShelter))
	{
		OutPolicy.HandlerId = TEXT("UseShelter"); OutPolicy.MaxRangeCm = 200.f;
		OutPolicy.AllowedTargetKinds.Add(Tags.Target_Kind_Shelter);
		OutPolicy.AllowedPhases = {EAuraBattlePhase::Alert, EAuraBattlePhase::Conflict, EAuraBattlePhase::Cleanup};
	}
	else if (OptionTag.MatchesTagExact(Tags.Interaction_Trade))
	{
		OutFailure = EAuraInteractionResultCode::FeatureUnavailable;
		return false;
	}
	else return false;

	if (!OutPolicy.AllowedTargetKinds.Contains(Targetable->GetAuraTargetKind()))
	{
		OutFailure = EAuraInteractionResultCode::WrongTargetKind; return false;
	}
	const UAuraCombatIdentityComponent* TargetIdentity = Targetable->GetAuraTargetIdentity();
	if (Targetable->GetAuraTargetKind() != Tags.Target_Kind_Shelter
		&& (!TargetIdentity || !TargetIdentity->HasValidIdentity() || !TargetIdentity->GetIdentity().bTargetable))
	{
		OutFailure = EAuraInteractionResultCode::InvalidTarget; return false;
	}
	if (Targetable->GetAuraTargetKind() == Tags.Target_Kind_Civilian
		&& (!TargetIdentity->GetIdentity().FactionTag.MatchesTagExact(Tags.Faction_Civilian)
			|| !TargetIdentity->GetIdentity().bCanBeDamaged
			|| !Cast<AAuraCharacterBase>(Target)
			|| !Cast<AAuraCharacterBase>(Target)->GetAppliedRoleState().IsValid()
			|| !Cast<AAuraCharacterBase>(Target)->GetAppliedRoleState().InteractionProfileTag.MatchesTagExact(Tags.Interaction_Civilian)))
	{
		OutFailure = EAuraInteractionResultCode::InvalidTarget; return false;
	}
	const UAuraCombatStateComponent* State = Targetable->GetAuraTargetState();
	if (Targetable->GetAuraTargetKind() != Tags.Target_Kind_Shelter && !State)
	{
		OutFailure = EAuraInteractionResultCode::InvalidTarget; return false;
	}
	if (State && !OutPolicy.AllowedLifeStates.Contains(State->GetLifeState()))
	{
		OutFailure = EAuraInteractionResultCode::WrongLifeState; return false;
	}
	const AAuraGameModeBase* GameMode = Target->GetWorld() ? Target->GetWorld()->GetAuthGameMode<AAuraGameModeBase>() : nullptr;
	const AAuraBattleDirector* Director = GameMode ? GameMode->GetBattleDirector() : nullptr;
	if (!Director || !OutPolicy.AllowedPhases.Contains(Director->GetCurrentPhase()))
	{
		OutFailure = EAuraInteractionResultCode::PhaseDenied; return false;
	}
	const UAuraBattleZoneConfig* ZoneConfig = Director->GetZoneConfig();
	const FAuraCombatPolicySnapshot ZoneSnapshot = ZoneConfig
		? ZoneConfig->BuildPolicySnapshot(Director, Target->GetActorLocation(), Director->GetCurrentPhase(), Director->GetActiveBattleEventId())
		: FAuraCombatPolicySnapshot();
	if (!ZoneSnapshot.IsValid()
		|| (!Targetable->GetAuraTargetZoneId().IsNone() && Targetable->GetAuraTargetZoneId() != ZoneSnapshot.GetBattleZoneId()))
	{
		OutFailure = EAuraInteractionResultCode::ZoneDenied; return false;
	}
	OutFailure = EAuraInteractionResultCode::Success;
	return true;
}
