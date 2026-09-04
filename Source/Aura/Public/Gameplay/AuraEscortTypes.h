// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

/** Results emitted by the inert Day 49 escort contract reducer. */
enum class EAuraEscortResult : uint8
{
	Rejected,
	Reserved,
	AlreadyReserved,
	WaitingForEscort,
	Sheltered,
	Moving,
	RetryRequested,
	ArrivalAccepted,
	DuplicateArrival,
	Completed,
	Failed,
	TechnicalAbort,
	Released,
	AlreadyReleased
};

enum class EAuraEscortPhase : uint8
{
	Idle,
	Preparing,
	Moving,
	WaitingForEscort,
	Sheltered,
	Completed,
	Failed,
	Aborted
};

/** Authority snapshot supplied by the existing population manager seam. */
struct FAuraEscortMemberSnapshot
{
	FName MemberId = NAME_None;
	int32 LifeGeneration = 0;
	bool bAlive = false;
	FGuid ExistingOwnerRunId;
	int32 ExistingOwnerEpoch = 0;
};

/** Destination lease bound to one stable civilian member identity. */
struct FAuraEscortReservation
{
	FName MemberId = NAME_None;
	int32 LifeGeneration = 0;
	FGuid OwnerRunId;
	int32 OwnerEpoch = 0;
	FName DestinationLeaseId = NAME_None;
};

/**
 * Testable authority-only reservation ledger. It models the resource seam
 * without mutating PopulationManager or creating a production component.
 */
class AURA_API FAuraEscortReservationLedger final
{
public:
	EAuraEscortResult TryReserve(bool bAuthority, const FGuid& InRunId, int32 InEpoch,
		const TArray<FName>& RequestedMemberIds, FName DestinationLeaseId,
		const TArray<FAuraEscortMemberSnapshot>& AvailableMembers,
		TArray<FAuraEscortReservation>& OutReservations, FString& OutError);

	EAuraEscortResult Release(bool bAuthority, const FGuid& InRunId, int32 InEpoch,
		const TArray<FName>& MemberIds, FString& OutError);

	bool IsReserved(FName MemberId) const { return ActiveReservations.Contains(MemberId); }
	bool IsWorkRestored(FName MemberId) const { return RestoredMemberIds.Contains(MemberId); }
	const FAuraEscortReservation* FindReservation(FName MemberId) const
	{
		return ActiveReservations.Find(MemberId);
	}

private:
	TMap<FName, FAuraEscortReservation> ActiveReservations;
	TSet<FName> RestoredMemberIds;
};

/** One arrangement's explicit, reviewable route/readability evidence. */
struct FAuraEscortLayoutDefinition
{
	FName ArrangementId = NAME_None;
	TArray<FName> ShelterWaypointIds;
	bool bAuraRouteReachable = false;
	bool bBungeeManRouteReachable = false;
	bool bCoverRegistered = false;
	bool bNavPathVerified = false;
	bool bEscortLOSVerified = false;
	bool bHubExcluded = false;
	bool bSpawnConstraintsVerified = false;
};

class AURA_API FAuraEscortLayoutRules final
{
public:
	static bool Validate(const TArray<FAuraEscortLayoutDefinition>& Layouts, FString& OutError);
};

/**
 * Inert Day 49 objective contract. All state transitions require the
 * authority bit and stable run/member identity supplied by the caller.
 */
class AURA_API FAuraEscortRunState final
{
public:
	bool Initialize(bool bAuthority, const FGuid& InRunId, int32 InEpoch, int32 InMissionGeneration,
		FName InMissionId, FName InArrangementId, double InStartServerTime, double InDeadlineSeconds,
		const TArray<FName>& InRequiredMemberIds, FString& OutError);

	EAuraEscortResult TryPrepare(bool bAuthority, FAuraEscortReservationLedger& Ledger,
		FName DestinationLeaseId, const TArray<FAuraEscortMemberSnapshot>& AvailableMembers,
		FString& OutError);

	EAuraEscortResult RegisterArrival(bool bAuthority, FName MemberId, int32 LifeGeneration,
		FName DestinationId, double AuthorityNow, FString& OutError);

	EAuraEscortResult TickMovement(bool bAuthority, double AuthorityNow, bool bParticipantWithinRange,
		bool bMovementRequested, bool bIntentionalShelterWait, bool bActualRouteProgress,
		FString& OutError);

	EAuraEscortResult RegisterCivilianDeath(bool bAuthority, FName MemberId, int32 LifeGeneration,
		double AuthorityNow, FString& OutError);

	bool CanMissionEnemyDamage(bool bAuthority, bool bMissionEnemySource, bool bPlayerSource,
		FName MemberId, int32 LifeGeneration, FString& OutError) const;

	EAuraEscortResult ReleaseReservations(bool bAuthority, FAuraEscortReservationLedger& Ledger,
		FString& OutError);

	const FGuid& GetRunId() const { return RunId; }
	int32 GetEpoch() const { return Epoch; }
	int32 GetMissionGeneration() const { return MissionGeneration; }
	FName GetMissionId() const { return MissionId; }
	FName GetArrangementId() const { return ArrangementId; }
	EAuraEscortPhase GetPhase() const { return Phase; }
	FName GetTerminalReason() const { return TerminalReason; }
	int32 GetArrivedCount() const { return ArrivedMemberIds.Num(); }
	int32 GetRequiredMemberCount() const { return RequiredMemberIds.Num(); }
	double GetStallSeconds() const { return AccumulatedStallSeconds; }
	int32 GetRetryCount() const { return RetryCount; }
	double GetLastActualProgressServerTime() const { return LastActualProgressServerTime; }
	double GetDeadlineServerTime() const { return DeadlineServerTime; }
	bool IsPrepared() const { return bPrepared; }
	bool AreReservationsReleased() const { return bReservationsReleased; }

private:
	bool IsRequiredMember(FName MemberId) const { return RequiredMemberIds.Contains(MemberId); }
	void Fail(FName Reason, bool bTechnical);

	FGuid RunId;
	int32 Epoch = 0;
	int32 MissionGeneration = 0;
	FName MissionId = NAME_None;
	FName ArrangementId = NAME_None;
	EAuraEscortPhase Phase = EAuraEscortPhase::Idle;
	FName TerminalReason = NAME_None;
	TArray<FName> RequiredMemberIds;
	TArray<FName> ReservedMemberIds;
	FName DestinationLeaseId = NAME_None;
	TSet<FName> ArrivedMemberIds;
	TSet<FName> DeadMemberIds;
	TMap<FName, int32> ReservedLifeGenerations;
	TMap<FName, bool> ReservedMemberAlive;
	double StartServerTime = 0.0;
	double DeadlineServerTime = 0.0;
	double LastMovementSampleServerTime = 0.0;
	double LastActualProgressServerTime = 0.0;
	double AccumulatedStallSeconds = 0.0;
	int32 RetryCount = 0;
	bool bAuthority = false;
	bool bPrepared = false;
	bool bReservationsReleased = false;
};
