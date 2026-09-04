// Copyright Druid Mechanics

#pragma once

#include "CoreMinimal.h"

enum class EAuraRunRecoveryMemberState : uint8
{
	Alive,
	AwaitingRescue,
	Recovering,
	Forfeited
};

enum class EAuraRunRecoveryResult : uint8
{
	Rejected,
	NoChange,
	DeathAccepted,
	DuplicateDeath,
	ChannelStarted,
	ChargeReserved,
	DuplicateAccepted,
	AttachmentRetryScheduled,
	Recovered,
	LeaseExpired,
	WipeFailed,
	TechnicalRecoveryFailure
};

/** Run-owned values that recovery must retain exactly; no default-resource refill is allowed. */
struct FAuraRunRecoveryRetainedState
{
	int32 MagazineRounds = 0;
	int32 ReserveRounds = 0;
	uint32 AmmoRevision = 0;
	int32 SupplyCount = 0;
	TArray<FName> AugmentIds;
	double AbilityCooldownEndServerTime = 0.0;
	double EvadeCooldownEndServerTime = 0.0;
	double SupplyCooldownEndServerTime = 0.0;

	bool operator==(const FAuraRunRecoveryRetainedState& Other) const;
};

struct FAuraRunRecoveryMember
{
	FName MemberId = NAME_None;
	FName RoleId = NAME_None;
	int32 LifeGeneration = 1;
	EAuraRunRecoveryMemberState State = EAuraRunRecoveryMemberState::Alive;
	bool bConnected = true;
	bool bTargetableDormantProxy = false;
	double DisconnectLeaseExpiryServerTime = 0.0;
	int32 DeathSequence = 0;
	FName BeaconId = NAME_None;
	FName ChannelOwnerId = NAME_None;
	double ChannelStartServerTime = 0.0;
	float HealthNormalized = 1.0f;
	float ManaNormalized = 1.0f;
	FAuraRunRecoveryRetainedState RetainedState;
};

struct FAuraRunRecoveryReceipt
{
	int32 Serial = 0;
	FName TargetMemberId = NAME_None;
	int32 DeathSequence = 0;
	int32 RetryCount = 0;
	double LastAttemptServerTime = 0.0;
	bool bCommitted = false;
	bool bTerminal = false;
};

struct FAuraRunRecoverySnapshot
{
	FGuid RunId;
	int32 Epoch = 0;
	int32 Revision = 0;
	int32 ChargesRemaining = 0;
	int32 ReservedCharges = 0;
	int32 AvailableCharges = 0;
	int32 BeaconCount = 0;
	int32 AlivePublicationCount = 0;
	int32 LegacySaveCount = 0;
	bool bTerminal = false;
	FName TerminalReason = NAME_None;
	TArray<FAuraRunRecoveryMember> Members;
	TArray<FAuraRunRecoveryReceipt> Receipts;

	bool operator==(const FAuraRunRecoverySnapshot& Other) const;
};

/**
 * Inert Day 51 authority reducer. It models recovery transactions without
 * touching combat life enums, actors, possession, ASC state, saves, or legacy respawn.
 */
class AURA_API FAuraRunRecoveryState final
{
public:
	bool Initialize(bool bAuthority, const FGuid& InRunId, int32 InEpoch, FName InGameplayProfile,
		int32 InParticipantCount, double StartServerTime, double RunDeadlineSeconds,
		const TArray<FAuraRunRecoveryMember>& InMembers, FString& OutError);

	EAuraRunRecoveryResult RegisterDeath(bool bAuthority, FName MemberId, int32 LifeGeneration,
		int32 DeathSequence, double AuthorityNow, FString& OutError);
	EAuraRunRecoveryResult BeginRecoveryChannel(bool bAuthority, FName RequesterMemberId,
		FName TargetMemberId, double AuthorityNow, FString& OutError);
	EAuraRunRecoveryResult TryReserveRecovery(bool bAuthority, FName RequesterMemberId,
		FName TargetMemberId, int32 DeathSequence, double AuthorityNow, int32& OutReceiptSerial,
		FString& OutError);
	EAuraRunRecoveryResult CommitRecoveryAttachment(bool bAuthority, int32 ReceiptSerial,
		bool bPawnCreated, bool bAscAttached, bool bLoadoutAttached, double AuthorityNow, FString& OutError);
	EAuraRunRecoveryResult SetConnected(bool bAuthority, FName MemberId, bool bConnected,
		double AuthorityNow, FString& OutError);
	EAuraRunRecoveryResult Tick(bool bAuthority, double AuthorityNow, FString& OutError);

	FAuraRunRecoverySnapshot BuildSnapshot() const;
	const TArray<FAuraRunRecoveryMember>& GetMembers() const { return Members; }
	static bool UsesCooperativeRecovery(FName GameplayProfile);

private:
	bool Touch(FString& OutError);
	int32 FindMember(FName MemberId) const;
	int32 FindReceipt(int32 Serial) const;
	int32 FindLiveReceipt(FName TargetMemberId, int32 DeathSequence) const;
	EAuraRunRecoveryResult EvaluateWipe(FString& OutError);

	FGuid RunId;
	int32 Epoch = 0;
	int32 Revision = 0;
	int32 ParticipantCount = 0;
	int32 ChargesRemaining = 0;
	int32 ReservedCharges = 0;
	int32 NextReceiptSerial = 1;
	int32 AlivePublicationCount = 0;
	double RunDeadlineServerTime = 0.0;
	bool bAuthority = false;
	bool bTerminal = false;
	FName TerminalReason = NAME_None;
	TArray<FAuraRunRecoveryMember> Members;
	TArray<FAuraRunRecoveryReceipt> Receipts;
};
