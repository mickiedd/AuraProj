// Copyright Druid Mechanics

#include "Gameplay/AuraEscortTypes.h"

namespace AuraEscortTypesPrivate
{
	void Reject(FString& OutError, const TCHAR* Message)
	{
		OutError = Message;
	}

	bool IsOwner(const FAuraEscortReservation& Reservation, const FGuid& RunId, int32 Epoch)
	{
		return Reservation.OwnerRunId == RunId && Reservation.OwnerEpoch == Epoch;
	}

	const FAuraEscortMemberSnapshot* FindMember(const TArray<FAuraEscortMemberSnapshot>& Members, FName MemberId)
	{
		return Members.FindByPredicate([MemberId](const FAuraEscortMemberSnapshot& Member)
		{
			return Member.MemberId == MemberId;
		});
	}
}

EAuraEscortResult FAuraEscortReservationLedger::TryReserve(bool bAuthority, const FGuid& InRunId, int32 InEpoch,
	const TArray<FName>& RequestedMemberIds, FName DestinationLeaseId,
	const TArray<FAuraEscortMemberSnapshot>& AvailableMembers,
	TArray<FAuraEscortReservation>& OutReservations, FString& OutError)
{
	OutError.Reset();
	OutReservations.Reset();
	if (!bAuthority)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReservationAuthorityRequired"));
		return EAuraEscortResult::Rejected;
	}
	if (!InRunId.IsValid() || InEpoch <= 0 || RequestedMemberIds.Num() != 2 || DestinationLeaseId.IsNone())
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReservationIdentityInvalid"));
		return EAuraEscortResult::Rejected;
	}
	TSet<FName> UniqueMemberIds;
	for (const FName MemberId : RequestedMemberIds)
	{
		if (MemberId.IsNone() || UniqueMemberIds.Contains(MemberId))
		{
			AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortMemberSetInvalid"));
			return EAuraEscortResult::Rejected;
		}
		UniqueMemberIds.Add(MemberId);
		const FAuraEscortMemberSnapshot* Member = AuraEscortTypesPrivate::FindMember(AvailableMembers, MemberId);
		if (!Member || Member->LifeGeneration <= 0 || !Member->bAlive)
		{
			AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortUnavailable"));
			return EAuraEscortResult::Rejected;
		}
		if (Member->ExistingOwnerRunId.IsValid()
			&& (Member->ExistingOwnerRunId != InRunId || Member->ExistingOwnerEpoch != InEpoch))
		{
			AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReservationConflict"));
			return EAuraEscortResult::Rejected;
		}
		if (const FAuraEscortReservation* Existing = ActiveReservations.Find(MemberId))
		{
			if (!AuraEscortTypesPrivate::IsOwner(*Existing, InRunId, InEpoch)
				|| Existing->LifeGeneration != Member->LifeGeneration
				|| Existing->DestinationLeaseId != DestinationLeaseId)
			{
				AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReservationConflict"));
				return EAuraEscortResult::Rejected;
			}
		}
	}

	bool bAllAlreadyReserved = true;
	for (const FName MemberId : RequestedMemberIds)
	{
		const FAuraEscortMemberSnapshot* Member = AuraEscortTypesPrivate::FindMember(AvailableMembers, MemberId);
		if (const FAuraEscortReservation* Existing = ActiveReservations.Find(MemberId))
		{
			OutReservations.Add(*Existing);
		}
		else
		{
			bAllAlreadyReserved = false;
			FAuraEscortReservation& Reservation = ActiveReservations.Add(MemberId);
			Reservation.MemberId = MemberId;
			Reservation.LifeGeneration = Member->LifeGeneration;
			Reservation.OwnerRunId = InRunId;
			Reservation.OwnerEpoch = InEpoch;
			Reservation.DestinationLeaseId = DestinationLeaseId;
			OutReservations.Add(Reservation);
		}
		RestoredMemberIds.Remove(MemberId);
	}
	return bAllAlreadyReserved ? EAuraEscortResult::AlreadyReserved : EAuraEscortResult::Reserved;
}

EAuraEscortResult FAuraEscortReservationLedger::Release(bool bAuthority, const FGuid& InRunId, int32 InEpoch,
	const TArray<FName>& MemberIds, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !InRunId.IsValid() || InEpoch <= 0 || MemberIds.Num() == 0)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReleaseIdentityInvalid"));
		return EAuraEscortResult::Rejected;
	}

	bool bHasActiveReservation = false;
	for (const FName MemberId : MemberIds)
	{
		if (const FAuraEscortReservation* Reservation = ActiveReservations.Find(MemberId))
		{
			if (!AuraEscortTypesPrivate::IsOwner(*Reservation, InRunId, InEpoch))
			{
				AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReleaseNotOwner"));
				return EAuraEscortResult::Rejected;
			}
			bHasActiveReservation = true;
		}
		else if (!RestoredMemberIds.Contains(MemberId))
		{
			AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReleaseNotReserved"));
			return EAuraEscortResult::Rejected;
		}
	}

	for (const FName MemberId : MemberIds)
	{
		ActiveReservations.Remove(MemberId);
		RestoredMemberIds.Add(MemberId);
	}
	return bHasActiveReservation ? EAuraEscortResult::Released : EAuraEscortResult::AlreadyReleased;
}

bool FAuraEscortLayoutRules::Validate(const TArray<FAuraEscortLayoutDefinition>& Layouts, FString& OutError)
{
	OutError.Reset();
	if (Layouts.Num() != 2)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortArrangementCountInvalid"));
		return false;
	}
	TSet<FName> Seen;
	for (const FAuraEscortLayoutDefinition& Layout : Layouts)
	{
		if (Layout.ArrangementId.IsNone() || Seen.Contains(Layout.ArrangementId)
			|| (Layout.ArrangementId != TEXT("arrangement_a") && Layout.ArrangementId != TEXT("arrangement_b"))
			|| Layout.ShelterWaypointIds.Num() != 2 || Layout.ShelterWaypointIds[0].IsNone()
			|| Layout.ShelterWaypointIds[1].IsNone() || Layout.ShelterWaypointIds[0] == Layout.ShelterWaypointIds[1]
			|| !Layout.bAuraRouteReachable || !Layout.bBungeeManRouteReachable || !Layout.bCoverRegistered
			|| !Layout.bNavPathVerified || !Layout.bEscortLOSVerified || Layout.bHubExcluded
			|| !Layout.bSpawnConstraintsVerified)
		{
			AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortArrangementInvalid"));
			return false;
		}
		Seen.Add(Layout.ArrangementId);
	}
	return Seen.Num() == 2;
}

bool FAuraEscortRunState::Initialize(bool bInAuthority, const FGuid& InRunId, int32 InEpoch,
	int32 InMissionGeneration, FName InMissionId, FName InArrangementId, double InStartServerTime,
	double InDeadlineSeconds, const TArray<FName>& InRequiredMemberIds, FString& OutError)
{
	OutError.Reset();
	*this = FAuraEscortRunState();
	if (!bInAuthority || !InRunId.IsValid() || InEpoch <= 0 || InMissionGeneration <= 0
		|| InMissionId.IsNone() || InArrangementId.IsNone() || !FMath::IsFinite(InStartServerTime)
		|| !FMath::IsFinite(InDeadlineSeconds) || InDeadlineSeconds <= 0.0
		|| InStartServerTime > TNumericLimits<double>::Max() - InDeadlineSeconds || InRequiredMemberIds.Num() != 2)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortRunIdentityInvalid"));
		return false;
	}
	TSet<FName> UniqueIds;
	for (const FName MemberId : InRequiredMemberIds)
	{
		if (MemberId.IsNone() || UniqueIds.Contains(MemberId))
		{
			AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortMemberSetInvalid"));
			return false;
		}
		UniqueIds.Add(MemberId);
	}

	bAuthority = true;
	RunId = InRunId;
	Epoch = InEpoch;
	MissionGeneration = InMissionGeneration;
	MissionId = InMissionId;
	ArrangementId = InArrangementId;
	Phase = EAuraEscortPhase::Preparing;
	RequiredMemberIds = InRequiredMemberIds;
	StartServerTime = InStartServerTime;
	DeadlineServerTime = InStartServerTime + InDeadlineSeconds;
	LastMovementSampleServerTime = InStartServerTime;
	LastActualProgressServerTime = InStartServerTime;
	return true;
}

EAuraEscortResult FAuraEscortRunState::TryPrepare(bool bInAuthority, FAuraEscortReservationLedger& Ledger,
	FName InDestinationLeaseId, const TArray<FAuraEscortMemberSnapshot>& AvailableMembers, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortPreparationRejected"));
		return EAuraEscortResult::Rejected;
	}
	if (bPrepared) return EAuraEscortResult::AlreadyReserved;
	if (Phase != EAuraEscortPhase::Preparing)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortPreparationRejected"));
		return EAuraEscortResult::Rejected;
	}
	if (InDestinationLeaseId.IsNone())
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortDestinationInvalid"));
		return EAuraEscortResult::Rejected;
	}

	TArray<FAuraEscortReservation> Reservations;
	const EAuraEscortResult Result = Ledger.TryReserve(true, RunId, Epoch, RequiredMemberIds,
		InDestinationLeaseId, AvailableMembers, Reservations, OutError);
	if (Result != EAuraEscortResult::Reserved && Result != EAuraEscortResult::AlreadyReserved)
	{
		return Result;
	}
	ReservedMemberIds.Reset();
	ReservedLifeGenerations.Reset();
	ReservedMemberAlive.Reset();
	for (const FAuraEscortReservation& Reservation : Reservations)
	{
		ReservedMemberIds.Add(Reservation.MemberId);
		ReservedLifeGenerations.Add(Reservation.MemberId, Reservation.LifeGeneration);
		ReservedMemberAlive.Add(Reservation.MemberId, true);
	}
	if (ReservedMemberIds.Num() != RequiredMemberIds.Num())
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReservationIncomplete"));
		return EAuraEscortResult::Rejected;
	}
	DestinationLeaseId = InDestinationLeaseId;
	bPrepared = true;
	bReservationsReleased = false;
	Phase = EAuraEscortPhase::Moving;
	return Result;
}

EAuraEscortResult FAuraEscortRunState::RegisterArrival(bool bInAuthority, FName MemberId, int32 LifeGeneration,
	FName DestinationId, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || !bPrepared || !FMath::IsFinite(AuthorityNow)
		|| MemberId.IsNone() || LifeGeneration <= 0 || DestinationId.IsNone())
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortArrivalInvalid"));
		return EAuraEscortResult::Rejected;
	}
	const int32* ExpectedLifeGeneration = ReservedLifeGenerations.Find(MemberId);
	if (!ExpectedLifeGeneration || *ExpectedLifeGeneration != LifeGeneration || DestinationId != DestinationLeaseId
		|| !IsRequiredMember(MemberId))
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortArrivalIdentityStale"));
		return EAuraEscortResult::Rejected;
	}
	if (ArrivedMemberIds.Contains(MemberId))
	{
		return EAuraEscortResult::DuplicateArrival;
	}
	if (Phase != EAuraEscortPhase::Moving && Phase != EAuraEscortPhase::WaitingForEscort
		&& Phase != EAuraEscortPhase::Sheltered)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortArrivalAfterTerminal"));
		return EAuraEscortResult::Rejected;
	}
	if (AuthorityNow > DeadlineServerTime + KINDA_SMALL_NUMBER)
	{
		Fail(TEXT("EscortDeadlineExpired"), false);
		return EAuraEscortResult::Failed;
	}
	ArrivedMemberIds.Add(MemberId);
	ReservedMemberAlive.FindOrAdd(MemberId) = true;
	if (ArrivedMemberIds.Num() == RequiredMemberIds.Num())
	{
		Phase = EAuraEscortPhase::Completed;
		return EAuraEscortResult::Completed;
	}
	return EAuraEscortResult::ArrivalAccepted;
}

EAuraEscortResult FAuraEscortRunState::TickMovement(bool bInAuthority, double AuthorityNow,
	bool bParticipantWithinRange, bool bMovementRequested, bool bIntentionalShelterWait,
	bool bActualRouteProgress, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || !bPrepared || !FMath::IsFinite(AuthorityNow)
		|| AuthorityNow < LastMovementSampleServerTime)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortMovementInvalid"));
		return EAuraEscortResult::Rejected;
	}
	if (Phase != EAuraEscortPhase::Moving && Phase != EAuraEscortPhase::WaitingForEscort
		&& Phase != EAuraEscortPhase::Sheltered)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortMovementAfterTerminal"));
		return EAuraEscortResult::Rejected;
	}
	if (AuthorityNow > DeadlineServerTime + KINDA_SMALL_NUMBER)
	{
		Fail(TEXT("EscortDeadlineExpired"), false);
		return EAuraEscortResult::Failed;
	}

	const bool bCountingMovement = bParticipantWithinRange && bMovementRequested && !bIntentionalShelterWait;
	if (bActualRouteProgress && bCountingMovement)
	{
		LastMovementSampleServerTime = AuthorityNow;
		LastActualProgressServerTime = AuthorityNow;
		AccumulatedStallSeconds = 0.0;
		RetryCount = 0;
		Phase = EAuraEscortPhase::Moving;
		return EAuraEscortResult::Moving;
	}
	if (!bCountingMovement)
	{
		LastMovementSampleServerTime = AuthorityNow;
		Phase = bIntentionalShelterWait ? EAuraEscortPhase::Sheltered : EAuraEscortPhase::WaitingForEscort;
		return bIntentionalShelterWait ? EAuraEscortResult::Sheltered : EAuraEscortResult::WaitingForEscort;
	}

	AccumulatedStallSeconds += AuthorityNow - LastMovementSampleServerTime;
	LastMovementSampleServerTime = AuthorityNow;
	Phase = EAuraEscortPhase::Moving;
	if (AccumulatedStallSeconds >= 9.0 - KINDA_SMALL_NUMBER)
	{
		Fail(TEXT("EscortPathStallAbort"), true);
		return EAuraEscortResult::TechnicalAbort;
	}
	if (RetryCount < 1 && AccumulatedStallSeconds >= 5.0 - KINDA_SMALL_NUMBER)
	{
		RetryCount = 1;
		return EAuraEscortResult::RetryRequested;
	}
	if (RetryCount < 2 && AccumulatedStallSeconds >= 7.0 - KINDA_SMALL_NUMBER)
	{
		RetryCount = 2;
		return EAuraEscortResult::RetryRequested;
	}
	return EAuraEscortResult::Moving;
}

EAuraEscortResult FAuraEscortRunState::RegisterCivilianDeath(bool bInAuthority, FName MemberId,
	int32 LifeGeneration, double AuthorityNow, FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || !bPrepared || !FMath::IsFinite(AuthorityNow)
		|| !IsRequiredMember(MemberId) || MemberId.IsNone())
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortDeathInvalid"));
		return EAuraEscortResult::Rejected;
	}
	const int32* ExpectedLifeGeneration = ReservedLifeGenerations.Find(MemberId);
	if (!ExpectedLifeGeneration || *ExpectedLifeGeneration != LifeGeneration)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortDeathIdentityStale"));
		return EAuraEscortResult::Rejected;
	}
	if (DeadMemberIds.Contains(MemberId)) return EAuraEscortResult::Rejected;
	if (Phase != EAuraEscortPhase::Moving && Phase != EAuraEscortPhase::WaitingForEscort
		&& Phase != EAuraEscortPhase::Sheltered)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortDeathAfterTerminal"));
		return EAuraEscortResult::Rejected;
	}
	DeadMemberIds.Add(MemberId);
	ReservedMemberAlive.FindOrAdd(MemberId) = false;
	Fail(AuthorityNow > DeadlineServerTime + KINDA_SMALL_NUMBER ? FName(TEXT("EscortDeadlineExpired"))
		: FName(TEXT("EscortCivilianLost")), false);
	return EAuraEscortResult::Failed;
}

bool FAuraEscortRunState::CanMissionEnemyDamage(bool bInAuthority, bool bMissionEnemySource, bool bPlayerSource,
	FName MemberId, int32 LifeGeneration, FString& OutError) const
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || !bPrepared || !bMissionEnemySource || bPlayerSource)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortDamagePolicyDenied"));
		return false;
	}
	const int32* ExpectedLifeGeneration = ReservedLifeGenerations.Find(MemberId);
	const bool* bAlive = ReservedMemberAlive.Find(MemberId);
	if (!ExpectedLifeGeneration || !bAlive || *ExpectedLifeGeneration != LifeGeneration || !*bAlive
		|| !IsRequiredMember(MemberId) || Phase == EAuraEscortPhase::Completed
		|| Phase == EAuraEscortPhase::Failed || Phase == EAuraEscortPhase::Aborted)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortTargetNotInRun"));
		return false;
	}
	return true;
}

EAuraEscortResult FAuraEscortRunState::ReleaseReservations(bool bInAuthority, FAuraEscortReservationLedger& Ledger,
	FString& OutError)
{
	OutError.Reset();
	if (!bAuthority || !bInAuthority || !bPrepared)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReleaseRejected"));
		return EAuraEscortResult::Rejected;
	}
	if (bReservationsReleased) return EAuraEscortResult::AlreadyReleased;
	if (Phase != EAuraEscortPhase::Completed && Phase != EAuraEscortPhase::Failed
		&& Phase != EAuraEscortPhase::Aborted)
	{
		AuraEscortTypesPrivate::Reject(OutError, TEXT("EscortReleaseBeforeTerminal"));
		return EAuraEscortResult::Rejected;
	}
	const EAuraEscortResult Result = Ledger.Release(true, RunId, Epoch, ReservedMemberIds, OutError);
	if (Result == EAuraEscortResult::Released || Result == EAuraEscortResult::AlreadyReleased)
	{
		bReservationsReleased = true;
	}
	return Result;
}

void FAuraEscortRunState::Fail(FName Reason, bool bTechnical)
{
	Phase = bTechnical ? EAuraEscortPhase::Aborted : EAuraEscortPhase::Failed;
	TerminalReason = Reason;
}
