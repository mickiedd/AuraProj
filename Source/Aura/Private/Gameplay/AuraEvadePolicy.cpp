// Copyright Druid Mechanics

#include "Gameplay/AuraEvadePolicy.h"

namespace AuraEvadePolicyPrivate
{
	void Reject(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}

	bool SameOwnerIdentity(const FAuraEvadeRequestKey& A, const FAuraEvadeRequestKey& B)
	{
		return A.RunId == B.RunId && A.Epoch == B.Epoch && A.OwnerId == B.OwnerId
			&& A.OwnerLifeGeneration == B.OwnerLifeGeneration;
	}

	void FillRejectedAcceptance(const FAuraEvadeRequest& Request, EAuraEvadeResult Result,
		FAuraEvadeAcceptance& OutAcceptance)
	{
		OutAcceptance = FAuraEvadeAcceptance();
		OutAcceptance.Result = Result;
		OutAcceptance.Key = Request.Key;
	}
}

bool FAuraEvadePolicy::IsNormalizedHorizontalDirection(const FVector& Direction, FVector& OutNormalizedDirection)
{
	OutNormalizedDirection = FVector::ZeroVector;
	if (!FMath::IsFinite(Direction.X) || !FMath::IsFinite(Direction.Y) || !FMath::IsFinite(Direction.Z)
		|| !FMath::IsNearlyZero(Direction.Z, 0.01f))
	{
		return false;
	}
	const FVector Horizontal(Direction.X, Direction.Y, 0.0f);
	const float SizeSquared = Horizontal.SizeSquared();
	if (!FMath::IsFinite(SizeSquared) || SizeSquared <= KINDA_SMALL_NUMBER
		|| !FMath::IsNearlyEqual(SizeSquared, 1.0f, 0.01f))
	{
		return false;
	}
	OutNormalizedDirection = Horizontal.GetSafeNormal();
	return FMath::IsNearlyEqual(OutNormalizedDirection.SizeSquared(), 1.0f, 0.01f);
}

EAuraEvadeResult FAuraEvadePolicy::TryAccept(const FAuraEvadeState& CurrentState,
	const FAuraEvadeRequest& Request, const FAuraEvadeOwnerSnapshot& OwnerSnapshot, bool bIsAuthority,
	double AuthorityNow, FAuraEvadeState& OutState, FAuraEvadeAcceptance& OutAcceptance, FString& OutError)
{
	OutError.Reset();
	OutState = CurrentState;
	AuraEvadePolicyPrivate::FillRejectedAcceptance(Request, EAuraEvadeResult::RejectedInvalidState, OutAcceptance);
	if (!bIsAuthority)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("NotAuthority"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedNotAuthority;
		return OutAcceptance.Result;
	}
	if (!Request.Key.IsValid() || !CurrentState.RunId.IsValid() || CurrentState.Epoch <= 0
		|| CurrentState.OwnerId.IsNone() || CurrentState.OwnerLifeGeneration <= 0
		|| CurrentState.MaxCharges != MaxCharges || CurrentState.ChargesRemaining < 0
		|| CurrentState.ChargesRemaining > CurrentState.MaxCharges)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("InvalidOwnerIdentity"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedInvalidOwner;
		return OutAcceptance.Result;
	}
	if (Request.Key.RunId != CurrentState.RunId || Request.Key.Epoch != CurrentState.Epoch
		|| Request.Key.OwnerId != CurrentState.OwnerId)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("StaleOwnerIdentity"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedStaleGeneration;
		return OutAcceptance.Result;
	}
	if (Request.Key.OwnerLifeGeneration != CurrentState.OwnerLifeGeneration)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("StaleOwnerLifeGeneration"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedStaleGeneration;
		return OutAcceptance.Result;
	}
	if (!FMath::IsFinite(AuthorityNow))
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("AuthorityTimeInvalid"));
		return OutAcceptance.Result;
	}
	if (CurrentState.bHasAcceptedRequest && Request.Key == CurrentState.LastAcceptedRequest)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("DuplicateEvadeRequest"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedDuplicate;
		return OutAcceptance.Result;
	}
	if (CurrentState.bHasAcceptedRequest && AuraEvadePolicyPrivate::SameOwnerIdentity(Request.Key, CurrentState.LastAcceptedRequest)
		&& Request.Key.RequestSequence <= CurrentState.LastAcceptedRequest.RequestSequence)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("EvadeSequenceReplay"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedDuplicate;
		return OutAcceptance.Result;
	}
	if (!OwnerSnapshot.bAlive || !OwnerSnapshot.bRunEligible)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("OwnerNotEligible"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedInvalidState;
		return OutAcceptance.Result;
	}

	if (AuthorityNow < OutState.CooldownReadyServerTime)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("EvadeCooldown"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedCooldown;
		return OutAcceptance.Result;
	}
	const int32 AvailableCharges = OutState.ChargesRemaining <= 0 ? OutState.MaxCharges : OutState.ChargesRemaining;
	if (AvailableCharges <= 0)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("EvadeNoCharge"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedNoCharge;
		return OutAcceptance.Result;
	}
	FVector NormalizedDirection;
	if (!IsNormalizedHorizontalDirection(Request.Direction, NormalizedDirection))
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("EvadeDirectionInvalid"));
		OutAcceptance.Result = EAuraEvadeResult::RejectedInvalidDirection;
		return OutAcceptance.Result;
	}

	OutState.ChargesRemaining = FMath::Max(0, AvailableCharges - 1);
	OutState.AcceptedAtServerTime = AuthorityNow;
	OutState.CooldownReadyServerTime = AuthorityNow + CooldownSeconds;
	OutState.ActiveUntilServerTime = AuthorityNow + DurationSeconds;
	OutState.bInProgress = true;
	OutState.bHasAcceptedRequest = true;
	OutState.LastAcceptedRequest = Request.Key;
	OutState.AcceptedRequestCount = FMath::Max(0, OutState.AcceptedRequestCount) + 1;
	OutState.bMovementResolved = false;
	OutState.bLastMovementBlocked = false;
	OutState.LastTravelDistanceUnits = 0.0f;

	OutAcceptance.Result = EAuraEvadeResult::Accepted;
	OutAcceptance.Key = Request.Key;
	OutAcceptance.Direction = NormalizedDirection;
	OutAcceptance.AcceptedAtServerTime = AuthorityNow;
	OutAcceptance.MovementEndServerTime = OutState.ActiveUntilServerTime;
	OutAcceptance.CooldownReadyServerTime = OutState.CooldownReadyServerTime;
	OutAcceptance.MaxDistanceUnits = MaxDistanceUnits;
	OutAcceptance.DurationSeconds = DurationSeconds;
	OutAcceptance.bConsumesCooldown = true;
	OutAcceptance.CancellationPlan.bCancelOwnedInteraction = true;
	OutAcceptance.CancellationPlan.bCancelReload = true;
	OutAcceptance.CancellationPlan.bCancelOwnedChannel = true;
	OutAcceptance.CancellationPlan.bGrantsInvulnerability = false;
	return OutAcceptance.Result;
}

EAuraEvadeSweepResult FAuraEvadePolicy::ResolveSweep(const FAuraEvadeState& CurrentState,
	const FAuraEvadeRequestKey& RequestKey, double AuthorityNow, float TravelDistanceUnits, bool bBlocked,
	FAuraEvadeState& OutState, FString& OutError)
{
	OutError.Reset();
	OutState = CurrentState;
	if (!FMath::IsFinite(AuthorityNow))
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("AuthorityTimeInvalid"));
		return EAuraEvadeSweepResult::RejectedNotAuthority;
	}
	if (!CurrentState.bHasAcceptedRequest || RequestKey != CurrentState.LastAcceptedRequest)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("UnknownEvadeRequest"));
		return EAuraEvadeSweepResult::RejectedUnknownRequest;
	}
	if (CurrentState.bMovementResolved)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("DuplicateEvadeSweep"));
		return EAuraEvadeSweepResult::RejectedDuplicate;
	}
	if (AuthorityNow < CurrentState.AcceptedAtServerTime - KINDA_SMALL_NUMBER
		|| AuthorityNow > CurrentState.ActiveUntilServerTime + KINDA_SMALL_NUMBER)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("EvadeMovementWindowExpired"));
		return EAuraEvadeSweepResult::RejectedOutsideMovementWindow;
	}
	if (!FMath::IsFinite(TravelDistanceUnits) || TravelDistanceUnits < 0.0f
		|| TravelDistanceUnits > MaxDistanceUnits + KINDA_SMALL_NUMBER)
	{
		AuraEvadePolicyPrivate::Reject(OutError, TEXT("EvadeTravelInvalid"));
		return EAuraEvadeSweepResult::RejectedInvalidTravel;
	}
	OutState.bMovementResolved = true;
	OutState.bInProgress = false;
	OutState.bLastMovementBlocked = bBlocked;
	OutState.LastTravelDistanceUnits = TravelDistanceUnits;
	return bBlocked ? EAuraEvadeSweepResult::AcceptedBlocked : EAuraEvadeSweepResult::AcceptedFullDistance;
}

bool FAuraEvadeState::Initialize(const FGuid& InRunId, int32 InEpoch, FName InOwnerId,
	int32 InOwnerLifeGeneration, int32 InMaxCharges)
{
	if (!InRunId.IsValid() || InEpoch <= 0 || InOwnerId.IsNone() || InOwnerLifeGeneration <= 0 || InMaxCharges <= 0)
	{
		return false;
	}
	Reset();
	RunId = InRunId;
	Epoch = InEpoch;
	OwnerId = InOwnerId;
	OwnerLifeGeneration = InOwnerLifeGeneration;
	MaxCharges = InMaxCharges;
	ChargesRemaining = InMaxCharges;
	return true;
}

void FAuraEvadeState::Reset()
{
	*this = FAuraEvadeState();
}
