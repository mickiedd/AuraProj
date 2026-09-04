// Copyright Druid Mechanics

#include "Gameplay/AuraEvadeComponent.h"

#include "GameFramework/Actor.h"

UAuraEvadeComponent::UAuraEvadeComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
	SetIsReplicatedByDefault(false);
}

bool UAuraEvadeComponent::InitializeAuthorityState(const FGuid& InRunId, int32 InEpoch, FName InOwnerId,
	int32 InOwnerLifeGeneration)
{
	return GetOwner() && GetOwner()->HasAuthority()
		&& State.Initialize(InRunId, InEpoch, InOwnerId, InOwnerLifeGeneration, FAuraEvadePolicy::MaxCharges);
}

EAuraEvadeResult UAuraEvadeComponent::TryAcceptRequest(const FAuraEvadeRequest& Request,
	const FAuraEvadeOwnerSnapshot& OwnerSnapshot, double AuthorityNow, FAuraEvadeAcceptance& OutAcceptance,
	FString& OutError)
{
	OutError.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutAcceptance = FAuraEvadeAcceptance();
		OutAcceptance.Result = EAuraEvadeResult::RejectedNotAuthority;
		OutAcceptance.Key = Request.Key;
		OutError = TEXT("NotAuthority");
		return OutAcceptance.Result;
	}
	FAuraEvadeState NewState;
	const EAuraEvadeResult Result = FAuraEvadePolicy::TryAccept(State, Request, OwnerSnapshot, true, AuthorityNow,
		NewState, OutAcceptance, OutError);
	if (Result == EAuraEvadeResult::Accepted) State = MoveTemp(NewState);
	return Result;
}

EAuraEvadeSweepResult UAuraEvadeComponent::ResolveSweep(const FAuraEvadeRequestKey& RequestKey,
	double AuthorityNow, float TravelDistanceUnits, bool bBlocked, FString& OutError)
{
	OutError.Reset();
	if (!GetOwner() || !GetOwner()->HasAuthority())
	{
		OutError = TEXT("NotAuthority");
		return EAuraEvadeSweepResult::RejectedNotAuthority;
	}
	FAuraEvadeState NewState;
	const EAuraEvadeSweepResult Result = FAuraEvadePolicy::ResolveSweep(State, RequestKey, AuthorityNow,
		TravelDistanceUnits, bBlocked, NewState, OutError);
	if (Result == EAuraEvadeSweepResult::AcceptedFullDistance || Result == EAuraEvadeSweepResult::AcceptedBlocked)
	{
		State = MoveTemp(NewState);
	}
	return Result;
}
