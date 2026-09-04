// Copyright Druid Mechanics

#include "Gameplay/AuraCombatStatusComponent.h"

#include "GameFramework/Actor.h"

UAuraCombatStatusComponent::UAuraCombatStatusComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UAuraCombatStatusComponent::HasAuthorityContext(FString& OutError) const
{
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutError = TEXT("NotAuthority");
		return false;
	}
	return true;
}

EAuraCombatStatusResult UAuraCombatStatusComponent::TryApplyStatus(const FAuraCombatStatusKey& Key,
	FName StackingPolicy, FName InterruptClassification, double AuthorityNow, double DurationSeconds,
	FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraCombatStatusResult::Rejected;
	return Ledger.TryApplyStatus(Key, StackingPolicy, InterruptClassification, AuthorityNow, DurationSeconds, OutError);
}

EAuraCombatStatusResult UAuraCombatStatusComponent::TryRemoveStatus(const FAuraCombatStatusKey& Key,
	EAuraCombatStatusRemovalReason Reason, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraCombatStatusResult::Rejected;
	return Ledger.TryRemoveStatus(Key, Reason, OutError);
}

bool UAuraCombatStatusComponent::TryBeginInterruptibleChannel(const FAuraInterruptChannelKey& Key,
	double AuthorityNow, double DurationSeconds, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return false;
	return Ledger.TryBeginInterruptibleChannel(Key, AuthorityNow, DurationSeconds, OutError);
}

EAuraInterruptResult UAuraCombatStatusComponent::TryInterruptChannel(const FAuraInterruptChannelKey& Key,
	int32 InterruptSequence, double AuthorityNow, FString& OutError)
{
	if (!HasAuthorityContext(OutError)) return EAuraInterruptResult::Rejected;
	return Ledger.TryInterruptChannel(Key, InterruptSequence, AuthorityNow, OutError);
}

int32 UAuraCombatStatusComponent::PruneExpired(double AuthorityNow)
{
	FString Error;
	return HasAuthorityContext(Error) ? Ledger.PruneExpired(AuthorityNow) : 0;
}
