// Copyright Druid Mechanics

#include "Gameplay/AuraAttackTimelineComponent.h"

#include "GameFramework/Actor.h"
#include "Net/UnrealNetwork.h"

UAuraAttackTimelineComponent::UAuraAttackTimelineComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(true);
}

void UAuraAttackTimelineComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
	Super::GetLifetimeReplicatedProps(OutLifetimeProps);
	DOREPLIFETIME(UAuraAttackTimelineComponent, State);
}

bool UAuraAttackTimelineComponent::HasAuthorityContext(FString& OutError) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutError = TEXT("NotAuthority");
		return false;
	}
	return true;
}

EAuraAttackTimelineResult UAuraAttackTimelineComponent::TryBeginWindup(const FAuraAttackTimelineKey& InKey,
	const FAuraAttackTimelineDefinition& Definition, double AuthorityNow, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraAttackTimelineResult::Rejected;
	return State.TryBeginWindup(InKey, Definition, AuthorityNow, OutError);
}

EAuraAttackTimelineResult UAuraAttackTimelineComponent::TryCommit(const FAuraAttackTimelineKey& CallbackKey,
	double AuthorityNow, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraAttackTimelineResult::Rejected;
	return State.TryCommit(CallbackKey, AuthorityNow, OutError);
}

EAuraAttackTimelineResult UAuraAttackTimelineComponent::TryResolveImpact(const FAuraAttackTimelineKey& CallbackKey,
	double AuthorityNow, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraAttackTimelineResult::Rejected;
	return State.TryResolveImpact(CallbackKey, AuthorityNow, OutError);
}

EAuraAttackTimelineResult UAuraAttackTimelineComponent::TryFinishRecovery(const FAuraAttackTimelineKey& CallbackKey,
	double AuthorityNow, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraAttackTimelineResult::Rejected;
	return State.TryFinishRecovery(CallbackKey, AuthorityNow, OutError);
}

EAuraAttackTimelineResult UAuraAttackTimelineComponent::TryCancel(const FAuraAttackTimelineKey& CallbackKey,
	FName Reason, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraAttackTimelineResult::Rejected;
	return State.TryCancel(CallbackKey, Reason, OutError);
}

void UAuraAttackTimelineComponent::ResetTimeline()
{
	if (GetOwner() && GetOwner()->HasAuthority()) State.Reset();
}

void UAuraAttackTimelineComponent::OnRep_State()
{
	// Presentation code will consume the replicated authoritative state later;
	// this foundation deliberately has no cue or damage side effects.
}
